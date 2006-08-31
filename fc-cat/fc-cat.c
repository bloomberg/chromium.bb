/*
 * $RCSId: xc/lib/fontconfig/fc-cache/fc-cache.c,v 1.8tsi Exp $
 *
 * Copyright © 2002 Keith Packard
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#else
#ifdef linux
#define HAVE_GETOPT_LONG 1
#endif
#define HAVE_GETOPT 1
#endif

#include "../src/fccache.c"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifndef HAVE_GETOPT
#define HAVE_GETOPT 0
#endif
#ifndef HAVE_GETOPT_LONG
#define HAVE_GETOPT_LONG 0
#endif

#if HAVE_GETOPT_LONG
#undef  _GNU_SOURCE
#define _GNU_SOURCE
#include <getopt.h>
const struct option longopts[] = {
    {"version", 0, 0, 'V'},
    {"verbose", 0, 0, 'v'},
    {"help", 0, 0, '?'},
    {NULL,0,0,0},
};
#else
#if HAVE_GETOPT
extern char *optarg;
extern int optind, opterr, optopt;
#endif
#endif

/*
 * POSIX has broken stdio so that getc must do thread-safe locking,
 * this is a serious performance problem for applications doing large
 * amounts of IO with getc (as is done here).  If available, use
 * the getc_unlocked varient instead.
 */
 
#if defined(getc_unlocked) || defined(_IO_getc_unlocked)
#define GETC(f) getc_unlocked(f)
#define PUTC(c,f) putc_unlocked(c,f)
#else
#define GETC(f) getc(f)
#define PUTC(c,f) putc(c,f)
#endif

FcBool
FcCachePrintSet (FcFontSet *set, FcStrSet *dirs, char *base_name);

static FcBool
FcCacheWriteChars (FILE *f, const FcChar8 *chars)
{
    FcChar8    c;
    while ((c = *chars++))
    {
	switch (c) {
	case '"':
	case '\\':
	    if (PUTC ('\\', f) == EOF)
		return FcFalse;
	    /* fall through */
	default:
	    if (PUTC (c, f) == EOF)
		return FcFalse;
	}
    }
    return FcTrue;
}

static FcBool
FcCacheWriteUlong (FILE *f, unsigned long t)
{
    int	    pow;
    unsigned long   temp, digit;

    temp = t;
    pow = 1;
    while (temp >= 10)
    {
	temp /= 10;
	pow *= 10;
    }
    temp = t;
    while (pow)
    {
	digit = temp / pow;
	if (PUTC ((char) digit + '0', f) == EOF)
	    return FcFalse;
	temp = temp - pow * digit;
	pow = pow / 10;
    }
    return FcTrue;
}

static FcBool
FcCacheWriteInt (FILE *f, int i)
{
    return FcCacheWriteUlong (f, (unsigned long) i);
}

static FcBool
FcCacheWriteStringOld (FILE *f, const FcChar8 *string)
{

    if (PUTC ('"', f) == EOF)
	return FcFalse;
    if (!FcCacheWriteChars (f, string))
	return FcFalse;
    if (PUTC ('"', f) == EOF)
	return FcFalse;
    return FcTrue;
}

static void
usage (char *program)
{
#if HAVE_GETOPT_LONG
    fprintf (stderr, "usage: %s [-V?] [--version] [--help] <fonts.cache-2>\n",
	     program);
#else
    fprintf (stderr, "usage: %s [-fsvV?] <fonts.cache-2>\n",
	     program);
#endif
    fprintf (stderr, "Reads font information caches in <fonts.cache-2>\n");
    fprintf (stderr, "\n");
#if HAVE_GETOPT_LONG
    fprintf (stderr, "  -V, --version        display font config version and exit\n");
    fprintf (stderr, "  -?, --help           display this help and exit\n");
#else
    fprintf (stderr, "  -V         (version) display font config version and exit\n");
    fprintf (stderr, "  -?         (help)    display this help and exit\n");
#endif
    exit (1);
}

static int
FcCacheFileOpen (char *cache_file, off_t *size)
{
    int fd;
    struct stat file_stat;

    fd = open(cache_file, O_RDONLY | O_BINARY);
    if (fd < 0)
        return -1;

    if (fstat (fd, &file_stat) < 0) {
	close (fd); 
	return -1;
    }
    *size = file_stat.st_size;
    return fd;
}

/*
 * return the path from the directory containing 'cache' to 'file'
 */

static const FcChar8 *
FcFileBaseName (const char *cache, const FcChar8 *file)
{
    const FcChar8   *cache_slash;
    int		    cache_len = strlen (cache);

    if (!strncmp (cache, file, cache_len) && file[cache_len] == '/')
	return file + cache_len + 1;
    return file;
}

FcBool
FcCachePrintSet (FcFontSet *set, FcStrSet *dirs, char *base_name)
{
    FcPattern	    *font;
    FcChar8	    *name, *dir;
    const FcChar8   *file, *base;
    int		    ret;
    int		    n;
    int		    id;
    FcStrList	    *list;

    list = FcStrListCreate (dirs);
    if (!list)
	goto bail2;
    
    while ((dir = FcStrListNext (list)))
    {
	base = FcFileBaseName (base_name, dir);
	if (!FcCacheWriteStringOld (stdout, base))
	    goto bail3;
	if (PUTC (' ', stdout) == EOF)
	    goto bail3;
	if (!FcCacheWriteInt (stdout, 0))
	    goto bail3;
        if (PUTC (' ', stdout) == EOF)
	    goto bail3;
	if (!FcCacheWriteStringOld (stdout, FC_FONT_FILE_DIR))
	    goto bail3;
	if (PUTC ('\n', stdout) == EOF)
	    goto bail3;
    }
    
    for (n = 0; n < set->nfont; n++)
    {
	FcPattern   **fonts = FcFontSetFonts (set);
	FcPattern   *encoded_font = fonts[n];
	FcPattern   *font = FcEncodedOffsetToPtr (set, encoded_font, FcPattern);

	if (FcPatternGetString (font, FC_FILE, 0, (FcChar8 **) &file) != FcResultMatch)
	    goto bail3;
	base = FcFileBaseName (base_name, file);
	if (FcPatternGetInteger (font, FC_INDEX, 0, &id) != FcResultMatch)
	    goto bail3;
	if (!FcCacheWriteStringOld (stdout, base))
	    goto bail3;
	if (PUTC (' ', stdout) == EOF)
	    goto bail3;
	if (!FcCacheWriteInt (stdout, id))
	    goto bail3;
        if (PUTC (' ', stdout) == EOF)
	    goto bail3;
	name = FcNameUnparse (font);
	if (!name)
	    goto bail3;
	ret = FcCacheWriteStringOld (stdout, name);
	FcStrFree (name);
	if (!ret)
	    goto bail3;
	if (PUTC ('\n', stdout) == EOF)
	    goto bail3;
    }
    
    FcStrListDone (list);

    return FcTrue;
    
bail3:
    FcStrListDone (list);
bail2:
    return FcFalse;
}

FcCache *
FcCacheFileMap (const FcChar8 *file)
{
    FcCache *cache;
    int	    fd;
    struct stat file_stat;

    fd = open (file, O_RDONLY | O_BINARY);
    if (fd < 0)
	return NULL;
    if (fstat (fd, &file_stat) < 0) {
	close (fd);
	return NULL;
    }
    if (FcCacheLoad (fd, file_stat.st_size, &cache)) {
	close (fd);
	return cache;
    }
    close (fd);
    return NULL;
}

int
main (int argc, char **argv)
{
    int		i;
    int		ret = 0;
    FcFontSet	*fs;
    FcStrSet    *dirs;
    FcCache	*cache;
    FcConfig	*config;
    int		verbose = 0;
#if HAVE_GETOPT_LONG || HAVE_GETOPT
    int		c;

#if HAVE_GETOPT_LONG
    while ((c = getopt_long (argc, argv, "Vv?", longopts, NULL)) != -1)
#else
    while ((c = getopt (argc, argv, "Vv?")) != -1)
#endif
    {
	switch (c) {
	case 'V':
	    fprintf (stderr, "fontconfig version %d.%d.%d\n", 
		     FC_MAJOR, FC_MINOR, FC_REVISION);
	    exit (0);
	case 'v':
	    verbose++;
	    break;
	default:
	    usage (argv[0]);
	}
    }
    i = optind;
#else
    i = 1;
#endif

    config = FcInitLoadConfig ();
    if (!config)
    {
	fprintf (stderr, "%s: Can't init font config library\n", argv[0]);
	return 1;
    }
    FcConfigSetCurrent (config);
    
    if (i >= argc)
        usage (argv[0]);

    for (; i < argc; i++)
    {
	int	j;
	off_t	size;
	intptr_t	*cache_dirs;
	
	if (FcFileIsDir ((const FcChar8 *)argv[i]))
	    cache = FcDirCacheMap ((const FcChar8 *) argv[i], config);
	else
	    cache = FcCacheFileMap (argv[i]);
	if (!cache)
	{
	    perror (argv[i]);
	    ret++;
	    continue;
	}
	
	dirs = FcStrSetCreate ();
	fs = FcCacheSet (cache);
	cache_dirs = FcCacheDirs (cache);
	for (j = 0; j < cache->dirs_count; j++) 
	    FcStrSetAdd (dirs, FcOffsetToPtr (cache_dirs,
					      cache_dirs[j],
					      FcChar8));

	if (verbose)
	    printf ("Name: %s\nDirectory: %s\n", argv[i], FcCacheDir(cache));
        FcCachePrintSet (fs, dirs, FcCacheDir (cache));

	FcStrSetDestroy (dirs);

	FcDirCacheUnmap (cache);
    }

    return 0;
}
