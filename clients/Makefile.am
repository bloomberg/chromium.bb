noinst_PROGRAMS = $(clients_programs) 		\
	$(poppler_programs) 			\
	$(simple_clients_programs)

libexec_PROGRAMS = $(desktop_shell)

if BUILD_SIMPLE_CLIENTS
simple_clients_programs = simple-egl simple-shm
simple_egl_SOURCES = simple-egl.c
simple_egl_LDADD = $(SIMPLE_CLIENT_LIBS) -lm

simple_shm_SOURCES = simple-shm.c
simple_shm_LDADD = $(SIMPLE_CLIENT_LIBS)
endif

if BUILD_CLIENTS
clients_programs =				\
	gears					\
	flower					\
	screenshot				\
	terminal				\
	image					\
	dnd					\
	smoke					\
	resizor					\
	wscreensaver				\
	eventdemo

desktop_shell = wayland-desktop-shell

noinst_LIBRARIES = libtoytoolkit.a

AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS =					\
	-DDATADIR='"$(datadir)"'		\
	$(CLIENT_CFLAGS)

libtoytoolkit_a_SOURCES =			\
	window.c				\
	window.h				\
	cairo-util.c				\
	cairo-util.h				\
	config.c

toolkit_libs =					\
	libtoytoolkit.a				\
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

wscreensaver_SOURCES = wscreensaver.c wscreensaver-glue.c glmatrix.c
wscreensaver_LDADD = $(toolkit_libs) -lGLU

eventdemo_SOURCES = eventdemo.c
eventdemo_LDADD = $(toolkit_libs)

wayland_desktop_shell_SOURCES =			\
	desktop-shell.c				\
	desktop-shell-client-protocol.h		\
	desktop-shell-protocol.c
wayland_desktop_shell_LDADD = $(toolkit_libs)

BUILT_SOURCES =					\
	screenshooter-client-protocol.h		\
	screenshooter-protocol.c		\
	desktop-shell-client-protocol.h		\
	desktop-shell-protocol.c

CLEANFILES = $(BUILT_SOURCES)
endif

@wayland_scanner_rules@

if HAVE_POPPLER
poppler_programs = view
view_SOURCES = view.c
view_LDADD = $(toolkit_libs) $(POPPLER_LIBS)
view_CPPFLAGS = $(AM_CPPFLAGS) $(POPPLER_CFLAGS)
endif
