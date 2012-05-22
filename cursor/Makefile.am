lib_LTLIBRARIES = libwayland-cursor.la

include_HEADERS = wayland-cursor.h

libwayland_cursor_la_SOURCES =			\
	wayland-cursor.c			\
	xcursor.c				\
	xcursor.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = wayland-cursor.pc

AM_CFLAGS = $(GCC_CFLAGS) -I../src
LDADD = $(top_builddir)/src/libwayland-client.la
