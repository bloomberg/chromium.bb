AUTOMAKE_OPTIONS=subdir-objects
include Makefile.sources

AM_CFLAGS = \
	$(WARN_CFLAGS) \
	$(VISIBILITY_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/freedreno \
	$(PTHREADSTUBS_CFLAGS) \
	-I$(top_srcdir)/include/drm

libdrm_freedreno_la_LTLIBRARIES = libdrm_freedreno.la
libdrm_freedreno_ladir = $(libdir)
libdrm_freedreno_la_LDFLAGS = -version-number 1:0:0 -no-undefined
libdrm_freedreno_la_LIBADD = ../libdrm.la @PTHREADSTUBS_LIBS@

libdrm_freedreno_la_SOURCES = $(LIBDRM_FREEDRENO_FILES)

libdrm_freedrenocommonincludedir = ${includedir}/freedreno
libdrm_freedrenocommoninclude_HEADERS = $(LIBDRM_FREEDRENO_H_FILES)

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libdrm_freedreno.pc

EXTRA_DIST = Android.mk
