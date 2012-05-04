#  Copyright 2005 Adam Jackson.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

DISTCHECK_CONFIGURE_FLAGS = --enable-nouveau-experimental-api --enable-radeon-experimental-api

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libdrm.pc

if HAVE_LIBKMS
LIBKMS_SUBDIR = libkms
endif

if HAVE_INTEL
INTEL_SUBDIR = intel
endif

if HAVE_NOUVEAU
NOUVEAU_SUBDIR = nouveau
endif

if HAVE_RADEON
RADEON_SUBDIR = radeon
endif

if HAVE_OMAP
OMAP_SUBDIR = omap
endif

if HAVE_EXYNOS
EXYNOS_SUBDIR = exynos
endif

SUBDIRS = . $(LIBKMS_SUBDIR) $(INTEL_SUBDIR) $(NOUVEAU_SUBDIR) $(RADEON_SUBDIR) $(OMAP_SUBDIR) $(EXYNOS_SUBDIR) tests include

libdrm_la_LTLIBRARIES = libdrm.la
libdrm_ladir = $(libdir)
libdrm_la_LDFLAGS = -version-number 2:4:0 -no-undefined
libdrm_la_LIBADD = @CLOCK_LIB@

libdrm_la_CPPFLAGS = -I$(top_srcdir)/include/drm

libdrm_la_SOURCES =				\
	xf86drm.c				\
	xf86drmHash.c				\
	xf86drmRandom.c				\
	xf86drmSL.c				\
	xf86drmMode.c				\
	xf86atomic.h				\
	libdrm_lists.h

libdrmincludedir = ${includedir}
libdrminclude_HEADERS = xf86drm.h xf86drmMode.h

EXTRA_DIST = libdrm.pc.in include/drm/*

copy-headers :
	cp -r $(kernel_source)/usr/include/drm $(top_srcdir)/include

commit-headers : copy-headers
	git add include
	git commit -am "Copy headers from kernel $$(GIT_DIR=$(kernel_source)/.git git describe)"
