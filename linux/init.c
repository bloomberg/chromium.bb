/* init.c -- Setup/Cleanup for DRM -*- linux-c -*-
 * Created: Mon Jan  4 08:58:31 1999 by faith@precisioninsight.com
 * Revised: Fri Aug 20 09:27:02 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * $PI: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/init.c,v 1.3 1999/08/20 15:07:01 faith Exp $
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/init.c,v 1.1 1999/09/25 14:38:01 dawes Exp $
 *
 */

#define __NO_VERSION__
#include "drmP.h"

int			      drm_flags		= 0;

/* drm_parse_option parses a single option.  See description for
   drm_parse_drm for details. */

static void drm_parse_option(char *s)
{
	char *c, *r;
	
	DRM_DEBUG("\"%s\"\n", s);
	if (!s || !*s) return;
	for (c = s; *c && *c != ':'; c++); /* find : or \0 */
	if (*c) r = c + 1; else r = NULL;  /* remember remainder */
	*c = '\0';			   /* terminate */
	if (!strcmp(s, "noctx")) {
		drm_flags |= DRM_FLAG_NOCTX;
		DRM_INFO("Server-mediated context switching OFF\n");
		return;
	}
	if (!strcmp(s, "debug")) {
		drm_flags |= DRM_FLAG_DEBUG;
		DRM_INFO("Debug messages ON\n");
		return;
	}
	DRM_ERROR("\"%s\" is not a valid option\n", s);
	return;
}

/* drm_parse_options parse the insmod "drm=" options, or the command-line
 * options passed to the kernel via LILO.  The grammar of the format is as
 * follows:
 *
 * drm		::= 'drm=' option_list
 * option_list	::= option [ ';' option_list ]
 * option	::= 'device:' major
 *		|   'debug' 
 *		|   'noctx'
 * major	::= INTEGER
 *
 * Note that 's' contains option_list without the 'drm=' part.
 *
 * device=major,minor specifies the device number used for /dev/drm
 *	  if major == 0 then the misc device is used
 *	  if major == 0 and minor == 0 then dynamic misc allocation is used
 * debug=on specifies that debugging messages will be printk'd
 * debug=trace specifies that each function call will be logged via printk
 * debug=off turns off all debugging options
 *
 */

void drm_parse_options(char *s)
{
	char *h, *t, *n;
	
	DRM_DEBUG("\"%s\"\n", s ?: "");
	if (!s || !*s) return;

	for (h = t = n = s; h && *h; h = n) {
		for (; *t && *t != ';'; t++);	       /* find ; or \0 */
		if (*t) n = t + 1; else n = NULL;      /* remember next */
		*t = '\0';			       /* terminate */
		drm_parse_option(h);		       /* parse */
	}
}

