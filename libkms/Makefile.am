AM_CFLAGS = \
	$(WARN_CFLAGS) \
	-I$(top_srcdir)/include/drm

libkms_la_LTLIBRARIES = libkms.la
libkms_ladir = $(libdir)
libkms_la_LDFLAGS = -version-number 1:0:0 -no-undefined
libkms_la_LIBADD = \
	$(LIBUDEV_LIBS)

libkms_la_SOURCES = \
	linux.c \
	api.c

if HAVE_VMWGFX
libkms_la_SOURCES += vmwgfx.c
endif

libkmsincludedir = ${includedir}/libkms
libkmsinclude_HEADERS = libkms.h

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libkms.pc

EXTRA_DIST = libkms.pc.in
