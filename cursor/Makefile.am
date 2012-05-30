lib_LTLIBRARIES = libwayland-cursor.la

include_HEADERS = wayland-cursor.h

libwayland_cursor_la_SOURCES =			\
	wayland-cursor.c			\
	xcursor.c				\
	xcursor.h
libwayland_cursor_la_LIBADD = $(top_builddir)/src/libwayland-client.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = wayland-cursor.pc

AM_CFLAGS = $(GCC_CFLAGS) -I$(top_builddir)/src -I$(top_srcdir)/src
