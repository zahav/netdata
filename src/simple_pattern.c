#include "common.h"

struct simple_pattern {
    const char *match;
    size_t len;

    SIMPLE_PREFIX_MODE mode;
    char negative;

    struct simple_pattern *child;

    struct simple_pattern *next;
};

static inline struct simple_pattern *parse_pattern(char *str, SIMPLE_PREFIX_MODE default_mode) {
    SIMPLE_PREFIX_MODE mode;
    struct simple_pattern *child = NULL;

    char *s = str, *c = str;

    // skip asterisks in front
    while(*c == '*') c++;

    // find the next asterisk
    while(*c && *c != '*') c++;

    // do we have an asterisk in the middle?
    if(*c == '*' && c[1] != '\0') {
        // yes, we have
        child = parse_pattern(c, default_mode);
        c[1] = '\0';
    }

    // check what this one matches

    size_t len = strlen(s);
    if(len >= 2 && *s == '*' && s[len - 1] == '*') {
        s[len - 1] = '\0';
        s++;
        mode = SIMPLE_PATTERN_SUBSTRING;
    }
    else if(len >= 1 && *s == '*') {
        s++;
        mode = SIMPLE_PATTERN_SUFFIX;
    }
    else if(len >= 1 && s[len - 1] == '*') {
        s[len - 1] = '\0';
        mode = SIMPLE_PATTERN_PREFIX;
    }
    else
        mode = default_mode;

    // allocate the structure
    struct simple_pattern *m = callocz(1, sizeof(struct simple_pattern));
    if(*s) {
        m->match = strdupz(s);
        m->len = strlen(m->match);
        m->mode = mode;
    }
    else {
        m->mode = SIMPLE_PATTERN_SUBSTRING;
    }

    m->child = child;

    return m;
}

SIMPLE_PATTERN *simple_pattern_create(const char *list, SIMPLE_PREFIX_MODE default_mode) {
    struct simple_pattern *root = NULL, *last = NULL;

    if(unlikely(!list || !*list)) return root;

    char *buf = mallocz(strlen(list) + 1);
    const char *s = list;

    while(s && *s) {
        buf[0] = '\0';
        char *c = buf;

        char negative = 0;

        // skip all spaces
        while(isspace(*s))
            s++;

        if(*s == '!') {
            negative = 1;
            s++;
        }

        // empty string
        if(unlikely(!*s))
            break;

        // find the next space
        char escape = 0;
        while(*s) {
            if(*s == '\\' && !escape) {
                escape = 1;
                s++;
            }
            else {
                if (isspace(*s) && !escape) {
                    s++;
                    break;
                }

                *c++ = *s++;
                escape = 0;
            }
        }

        // terminate our string
        *c = '\0';

        // if we matched the empty string, skip it
        if(!*buf)
            continue;

        struct simple_pattern *m = parse_pattern(buf, default_mode);
        m->negative = negative;

        // link it at the end
        if(unlikely(!root))
            root = last = m;
        else {
            last->next = m;
            last = m;
        }
    }

    freez(buf);
    return (SIMPLE_PATTERN *)root;
}

static inline int match_pattern(struct simple_pattern *m, const char *str, size_t len) {
    char *s;

    if(m->len <= len) {
        switch(m->mode) {
            case SIMPLE_PATTERN_SUBSTRING:
                if(!m->len) return 1;
                if((s = strstr(str, m->match))) {
                    if(!m->child) return 1;
                    return match_pattern(m->child, &s[m->len], len - (s - str) - m->len);
                }
                break;

            case SIMPLE_PATTERN_PREFIX:
                if(unlikely(strncmp(str, m->match, m->len) == 0)) {
                    if(!m->child) return 1;
                    return match_pattern(m->child, &str[m->len], len - m->len);
                }
                break;

            case SIMPLE_PATTERN_SUFFIX:
                if(unlikely(strcmp(&str[len - m->len], m->match) == 0)) {
                    if(!m->child) return 1;
                    return 0;
                }
                break;

            case SIMPLE_PATTERN_EXACT:
            default:
                if(unlikely(strcmp(str, m->match) == 0)) {
                    if(!m->child) return 1;
                    return 0;
                }
                break;
        }
    }

    return 0;
}

int simple_pattern_matches(SIMPLE_PATTERN *list, const char *str) {
    struct simple_pattern *m, *root = (struct simple_pattern *)list;

    if(unlikely(!root || !str || !*str)) return 0;

    size_t len = strlen(str);
    for(m = root; m ; m = m->next)
        if(match_pattern(m, str, len)) {
            if(m->negative) return 0;
            return 1;
        }

    return 0;
}

static inline void free_pattern(struct simple_pattern *m) {
    if(!m) return;

    free_pattern(m->child);
    free_pattern(m->next);
    freez((void *)m->match);
    freez(m);
}

void simple_pattern_free(SIMPLE_PATTERN *list) {
    if(!list) return;

    free_pattern(((struct simple_pattern *)list));
}
