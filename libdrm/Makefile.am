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

libdrm_la_LTLIBRARIES = libdrm.la
libdrm_ladir = $(libdir)
libdrm_la_LDFLAGS = -version-number 1:0:0 -no-undefined

AM_CFLAGS = -I$(top_srcdir)/shared-core
libdrm_la_SOURCES = xf86drm.c xf86drmHash.c xf86drmRandom.c xf86drmSL.c

libdrmincludedir = ${includedir}
libdrminclude_HEADERS = xf86drm.h

klibdrmincludedir = ${includedir}/drm
klibdrminclude_HEADERS = \
                         ../shared-core/drm.h \
                         ../shared-core/drm_sarea.h \
                         ../shared-core/i915_drm.h \
                         ../shared-core/i915_drv.h \
                         ../shared-core/imagine_drv.h \
                         ../shared-core/mach64_drm.h \
                         ../shared-core/mach64_drv.h \
                         ../shared-core/mga_drm.h \
                         ../shared-core/mga_drv.h \
                         ../shared-core/mga_ucode.h \
                         ../shared-core/r128_drm.h \
                         ../shared-core/r128_drv.h \
                         ../shared-core/radeon_drm.h \
                         ../shared-core/radeon_drv.h \
                         ../shared-core/savage_drm.h \
                         ../shared-core/savage_drv.h \
                         ../shared-core/sis_drm.h \
                         ../shared-core/sis_drv.h \
                         ../shared-core/sis_ds.h \
                         ../shared-core/tdfx_drv.h \
                         ../shared-core/via_3d_reg.h \
                         ../shared-core/via_drm.h \
                         ../shared-core/via_drv.h \
                         ../shared-core/via_ds.h \
                         ../shared-core/via_mm.h \
                         ../shared-core/via_verifier.h
