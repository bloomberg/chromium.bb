/*
 * $Id$
 *
 * Copyright © 2003 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

typedef enum { False, True } Bool;

typedef struct {
    char    *buf;
    int	    size;
    int	    len;
} String;

#define STRING_INIT 128

void *
New (int size)
{
    void    *m = malloc (size);
    if (!m)
	abort ();
    return m;
}

void *
Reallocate (void *p, int size)
{
    void    *r = realloc (p, size);

    if (!r)
	abort ();
    return r;
}

void
Dispose (void *p)
{
    free (p);
}

String *
StringNew (void)
{
    String  *s;

    s = New (sizeof (String));
    s->buf = New (STRING_INIT);
    s->size = STRING_INIT - 1;
    s->buf[0] = '\0';
    s->len = 0;
    return s;
}

void
StringAdd (String *s, char c)
{
    if (s->len == s->size)
	s->buf = Reallocate (s->buf, (s->size *= 2) + 1);
    s->buf[s->len++] = c;
    s->buf[s->len] = '\0';
}

void
StringAddString (String *s, char *buf)
{
    while (*buf)
	StringAdd (s, *buf++);
}

String *
StringMake (char *buf)
{
    String  *s = StringNew ();
    StringAddString (s, buf);
    return s;
}

void
StringDel (String *s)
{
    if (s->len)
	s->buf[--s->len] = '\0';
}

void
StringPut (FILE *f, String *s)
{
    char    *b = s->buf;

    while (*b)
	putc (*b++, f);
}

#define StringLast(s)	((s)->len ? (s)->buf[(s)->len - 1] : '\0')

void
StringDispose (String *s)
{
    Dispose (s->buf);
    Dispose (s);
}

typedef struct {
    String  *tag;
    String  *text;
} Replace;

Replace *
ReplaceNew (void)
{
    Replace *r = New (sizeof (Replace));
    r->tag = StringNew ();
    r->text = StringNew ();
    return r;
}

void
ReplaceDispose (Replace *r)
{
    StringDispose (r->tag);
    StringDispose (r->text);
    Dispose (r);
}

void
Bail (char *format, char *arg)
{
    fprintf (stderr, "fatal: ");
    fprintf (stderr, format, arg);
    fprintf (stderr, "\n");
    exit (1);
}

Replace *
ReplaceRead (FILE *f)
{
    int	    c;
    Replace *r;

    while ((c = getc (f)) != '@')
    {
	if (c == EOF)
	    return 0;
    }
    r = ReplaceNew();
    while ((c = getc (f)) != '@')
    {
	if (c == EOF)
	{
	    ReplaceDispose (r);
	    return 0;
	}
	if (isspace (c))
	    Bail ("invalid character after tag %s", r->tag->buf);
	StringAdd (r->tag, c);
    }
    if (r->tag->buf[0] == '\0')
    {
	ReplaceDispose (r);
	return 0;
    }
    while (isspace ((c = getc (f))))
	;
    ungetc (c, f);
    while ((c = getc (f)) != '@' && c != EOF)
	StringAdd (r->text, c);
    if (c == '@')
	ungetc (c, f);
    while (StringLast (r->text) == '\n')
	StringDel (r->text);
    return r;
}

typedef struct _replaceList {
    struct _replaceList	*next;
    Replace		*r;
} ReplaceList;

ReplaceList *
ReplaceListNew (Replace *r, ReplaceList *next)
{
    ReplaceList	*l = New (sizeof (ReplaceList));
    l->r = r;
    l->next = next;
    return l;
}

void
ReplaceListDispose (ReplaceList *l)
{
    if (l)
    {
	ReplaceListDispose (l->next);
	ReplaceDispose (l->r);
	Dispose (l);
    }
}

typedef struct {
    ReplaceList	*head;
} ReplaceSet;

ReplaceSet *
ReplaceSetNew (void)
{
    ReplaceSet	*s = New (sizeof (ReplaceSet));
    s->head = 0;
    return s;
}

void
ReplaceSetDispose (ReplaceSet *s)
{
    ReplaceListDispose (s->head);
    Dispose (s);
}

void
ReplaceSetAdd (ReplaceSet *s, Replace *r)
{
    s->head = ReplaceListNew (r, s->head);
}

Replace *
ReplaceSetFind (ReplaceSet *s, char *tag)
{
    ReplaceList	*l;

    for (l = s->head; l; l = l->next)
	if (!strcmp (tag, l->r->tag->buf))
	    return l->r;
    return 0;
}

ReplaceSet *
ReplaceSetRead (FILE *f)
{
    ReplaceSet	*s = ReplaceSetNew ();
    Replace	*r;

    while ((r = ReplaceRead (f)))
    {
	while (ReplaceSetFind (s, r->tag->buf))
	    StringAdd (r->tag, '+');
	ReplaceSetAdd (s, r);
    }
    if (!s->head)
    {
	ReplaceSetDispose (s);
	s = 0;
    }
    return s;
}

typedef struct _skipStack {
    struct _skipStack	*prev;
    int			skipping;
} SkipStack;

SkipStack *
SkipStackPush (SkipStack *prev, int skipping)
{
    SkipStack	*ss = New (sizeof (SkipStack));
    ss->prev = prev;
    ss->skipping = skipping;
    return ss;
}

SkipStack *
SkipStackPop (SkipStack *prev)
{
    SkipStack	*ss = prev->prev;
    Dispose (prev);
    return ss;
}

typedef struct _loopStack {
    struct _loopStack	*prev;
    String		*tag;
    String		*extra;
    long		pos;
} LoopStack;

LoopStack *
LoopStackPush (LoopStack *prev, FILE *f, char *tag)
{
    LoopStack	*ls = New (sizeof (LoopStack));
    ls->prev = prev;
    ls->tag = StringMake (tag);
    ls->extra = StringNew ();
    ls->pos = ftell (f);
    return ls;
}

LoopStack *
LoopStackLoop (ReplaceSet *rs, LoopStack *ls, FILE *f)
{
    String	*s = StringMake (ls->tag->buf);
    LoopStack	*ret = ls;
    Bool	loop;

    StringAdd (ls->extra, '+');
    StringAddString (s, ls->extra->buf);
    loop = ReplaceSetFind (rs, s->buf) != 0;
    StringDispose (s);
    if (loop)
	fseek (f, ls->pos, SEEK_SET);
    else
    {
	ret = ls->prev;
	StringDispose (ls->tag);
	StringDispose (ls->extra);
	Dispose (ls);
    }
    return ret;
}

void
LineSkip (FILE *f)
{
    int	c;

    while ((c = getc (f)) == '\n')
	;
    ungetc (c, f);
}

void
DoReplace (FILE *f, ReplaceSet *s)
{
    int		c;
    String	*tag;
    Replace	*r;
    SkipStack	*ss = 0;
    LoopStack	*ls = 0;
    int		skipping = 0;

    while ((c = getc (f)) != EOF)
    {
	if (c == '@')
	{
	    tag = StringNew ();
	    while ((c = getc (f)) != '@')
	    {
		if (c == EOF)
		    abort ();
		StringAdd (tag, c);
	    }
	    if (ls)
		StringAddString (tag, ls->extra->buf);
	    switch (tag->buf[0]) {
	    case '?':
		ss = SkipStackPush (ss, skipping);
		if (!ReplaceSetFind (s, tag->buf + 1))
		    skipping++;
		LineSkip (f);
		break;
	    case ':':
		if (!ss)
		    abort ();
		if (ss->skipping == skipping)
		    ++skipping;
		else
		    --skipping;
		LineSkip (f);
		break;
	    case ';':
		skipping = ss->skipping;
		ss = SkipStackPop (ss);
		LineSkip (f);
		break;
	    case '{':
		ls = LoopStackPush (ls, f, tag->buf + 1);
		LineSkip (f);
		break;
	    case '}':
		ls = LoopStackLoop (s, ls, f);
		LineSkip (f);
		break;
	    default:
		r = ReplaceSetFind (s, tag->buf);
		if (r && !skipping)
		    StringPut (stdout, r->text);
		break;
	    }
	    StringDispose (tag);
	}
	else if (!skipping)
	    putchar (c);
    }
}

int
main (int argc, char **argv)
{
    FILE	*f;
    ReplaceSet	*s;

    if (!argv[1])
	Bail ("usage: %s <template.sgml>", argv[0]);
    f = fopen (argv[1], "r");
    if (!f)
    {
	Bail ("can't open file %s", argv[1]);
	exit (1);
    }
    while ((s = ReplaceSetRead (stdin)))
    {
	DoReplace (f, s);
	ReplaceSetDispose (s);
	rewind (f);
    }
    if (ferror (stdout))
	Bail ("%s", "error writing output");
    exit (0);
}
