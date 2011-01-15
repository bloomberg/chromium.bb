noinst_PROGRAMS =				\
	gears					\
	flower					\
	screenshot				\
	terminal				\
	image					\
	$(poppler_programs)			\
	dnd					\
	smoke					\
	resizor					\
	simple-client

noinst_LTLIBRARIES = libtoytoolkit.la

AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS =					\
	-DDATADIR='"$(datadir)"'		\
	-I$(top_srcdir)/wayland			\
	-I$(top_builddir)/wayland		\
	$(CLIENT_CFLAGS)

libtoytoolkit_la_SOURCES =			\
	window.c				\
	window.h				\
	wayland-glib.c				\
	wayland-glib.h				\
	cairo-util.c				\
	cairo-util.h

toolkit_libs =						\
	libtoytoolkit.la				\
	$(top_builddir)/wayland/libwayland-client.la	\
	$(CLIENT_LIBS) -lrt -lm

gears_SOURCES = gears.c
gears_LDADD = $(toolkit_libs)

flower_SOURCES = flower.c
flower_LDADD = $(toolkit_libs)

screenshot_SOURCES = screenshot.c screenshooter-protocol.c
screenshot_LDADD = $(toolkit_libs)

terminal_SOURCES = terminal.c
terminal_LDADD = $(toolkit_libs) -lutil

image_SOURCES = image.c
image_LDADD = $(toolkit_libs)

dnd_SOURCES = dnd.c
dnd_LDADD = $(toolkit_libs)

smoke_SOURCES = smoke.c
smoke_LDADD = $(toolkit_libs)

resizor_SOURCES = resizor.c
resizor_LDADD = $(toolkit_libs)

simple_client_SOURCES = simple-client.c
simple_client_LDADD = \
	$(top_builddir)/wayland/libwayland-client.la -lm $(GLES2_LIBS)

BUILT_SOURCES =					\
	screenshooter-client-protocol.h		\
	screenshooter-protocol.c

CLEANFILES = $(BUILT_SOURCES)

include $(top_srcdir)/wayland/scanner.mk

if HAVE_POPPLER
poppler_programs = view
view_SOURCES = view.c
view_LDADD = $(toolkit_libs) $(POPPLER_LIBS)
view_CPPFLAGS = $(AM_CPPFLAGS) $(POPPLER_CFLAGS)
endif
