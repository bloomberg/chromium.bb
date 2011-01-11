noinst_PROGRAMS =				\
	gears					\
	flower					\
	screenshot				\
	terminal				\
	image					\
	$(poppler_programs)			\
	dnd					\
	smoke					\
	resizor

noinst_LTLIBRARIES = libtoytoolkit.la

INCLUDES =					\
	-I$(top_srcdir)/wayland			\
	-I$(top_builddir)/wayland			\
	$(CLIENT_CFLAGS)

LDADD = libtoytoolkit.la				\
	$(top_builddir)/wayland/libwayland-client.la	\
	$(CLIENT_LIBS) -lrt -lm

AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS = -DDATADIR='"$(datadir)"'

libtoytoolkit_la_SOURCES =			\
	window.c				\
	window.h				\
	wayland-glib.c				\
	wayland-glib.h				\
	cairo-util.c				\
	cairo-util.h

flower_SOURCES = flower.c
smoke_SOURCES = smoke.c
gears_SOURCES = gears.c
screenshot_SOURCES = screenshot.c screenshooter-protocol.c
terminal_SOURCES = terminal.c
image_SOURCES = image.c
dnd_SOURCES = dnd.c
resizor_SOURCES = resizor.c

BUILT_SOURCES =					\
	screenshooter-client-protocol.h		\
	screenshooter-protocol.c

CLEANFILES = $(BUILT_SOURCES)

include $(top_srcdir)/wayland/scanner.mk

terminal_LDADD = $(LDADD) -lutil

if HAVE_POPPLER
poppler_programs = view
view_SOURCES = view.c
view_LDADD = $(LDADD) $(POPPLER_LIBS)
view_CPPFLAGS = $(POPPLER_CFLAGS)
endif
