AM_CFLAGS = \
	$(WARN_CFLAGS) \
	-I$(top_srcdir)/include/drm \
	-I$(top_srcdir)

libkms_la_LTLIBRARIES = libkms.la
libkms_ladir = $(libdir)
libkms_la_LDFLAGS = -version-number 1:0:0 -no-undefined
libkms_la_LIBADD =

#if HAVE_LIBUDEV
#libkms_la_LIBADD += $(LIBUDEV_LIBS)
#endif

libkms_la_SOURCES = \
	internal.h \
	linux.c \
	intel.c \
	dumb.c \
	api.c

if HAVE_VMWGFX
libkms_la_SOURCES += vmwgfx.c
endif

if HAVE_NOUVEAU
libkms_la_SOURCES += nouveau.c
endif

if HAVE_RADEON
libkms_la_SOURCES += radeon.c
endif

libkmsincludedir = ${includedir}/libkms
libkmsinclude_HEADERS = libkms.h

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libkms.pc

EXTRA_DIST = libkms.pc.in
