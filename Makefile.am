if BUILD_DOCS
doc_subdir = doc
endif

SUBDIRS = src $(doc_subdir) tests

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

aclocaldir = $(datadir)/aclocal
dist_aclocal_DATA = wayland-scanner.m4

dist_pkgdata_DATA =				\
	wayland-scanner.mk			\
	protocol/wayland.xml			\
	protocol/wayland.dtd

lib_LTLIBRARIES = libwayland-cursor.la

include_HEADERS = cursor/wayland-cursor.h

libwayland_cursor_la_SOURCES =			\
	cursor/wayland-cursor.c			\
	cursor/os-compatibility.c		\
	cursor/os-compatibility.h		\
	cursor/cursor-data.h			\
	cursor/xcursor.c			\
	cursor/xcursor.h
libwayland_cursor_la_LIBADD = src/libwayland-client.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cursor/wayland-cursor.pc

libwayland_cursor_la_CFLAGS =			\
	$(GCC_CFLAGS)				\
	-I$(top_builddir)/src			\
	-I$(top_srcdir)/src			\
	-DICONDIR=\"$(ICONDIR)\"
