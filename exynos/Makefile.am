AM_CFLAGS = \
	$(WARN_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/exynos \
	$(PTHREADSTUBS_CFLAGS) \
	-I$(top_srcdir)/include/drm

libdrm_exynos_la_LTLIBRARIES = libdrm_exynos.la
libdrm_exynos_ladir = $(libdir)
libdrm_exynos_la_LDFLAGS = -version-number 1:0:0 -no-undefined
libdrm_exynos_la_LIBADD = ../libdrm.la @PTHREADSTUBS_LIBS@

libdrm_exynos_la_SOURCES = exynos_drm.c

libdrm_exynoscommonincludedir = ${includedir}/exynos
libdrm_exynoscommoninclude_HEADERS = exynos_drm.h

libdrm_exynosincludedir = ${includedir}/libdrm
libdrm_exynosinclude_HEADERS = exynos_drmif.h

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libdrm_exynos.pc
