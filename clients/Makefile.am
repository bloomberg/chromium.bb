bin_PROGRAMS =					\
	weston-info				\
	$(terminal)

noinst_PROGRAMS =				\
	$(clients_programs)			\
	$(pango_programs)			\
	$(poppler_programs)			\
	$(simple_clients_programs)		\
	$(simple_egl_clients_programs)

libexec_PROGRAMS =				\
	$(desktop_shell)			\
	$(tablet_shell)				\
	$(screenshooter)			\
	$(screensaver)				\
	$(keyboard)				\
	weston-simple-im

AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS =					\
	-DDATADIR='"$(datadir)"'		\
	-DBINDIR='"$(bindir)"'			\
	$(CLIENT_CFLAGS) $(CAIRO_EGL_CFLAGS)

if BUILD_SIMPLE_CLIENTS
simple_clients_programs =			\
	simple-shm				\
	simple-touch

simple_shm_SOURCES = simple-shm.c 		\
	../shared/os-compatibility.c		\
	../shared/os-compatibility.h
simple_shm_CPPFLAGS = $(SIMPLE_CLIENT_CFLAGS)
simple_shm_LDADD = $(SIMPLE_CLIENT_LIBS)

simple_touch_SOURCES = simple-touch.c		\
	../shared/os-compatibility.c		\
	../shared/os-compatibility.h
simple_touch_CPPFLAGS = $(SIMPLE_CLIENT_CFLAGS)
simple_touch_LDADD = $(SIMPLE_CLIENT_LIBS)
endif

if BUILD_SIMPLE_EGL_CLIENTS
simple_egl_clients_programs =			\
	simple-egl

simple_egl_SOURCES = simple-egl.c
simple_egl_CPPFLAGS = $(SIMPLE_EGL_CLIENT_CFLAGS)
simple_egl_LDADD = $(SIMPLE_EGL_CLIENT_LIBS) -lm
endif

if BUILD_CLIENTS
terminal = weston-terminal

clients_programs =				\
	flower					\
	image					\
	cliptest				\
	dnd					\
	smoke					\
	resizor					\
	eventdemo				\
	clickdot				\
	transformed				\
	calibrator				\
	$(full_gl_client_programs)

desktop_shell = weston-desktop-shell

if ENABLE_TABLET_SHELL
tablet_shell = weston-tablet-shell
endif

screenshooter = weston-screenshooter

noinst_LTLIBRARIES = libtoytoolkit.la

libtoytoolkit_la_SOURCES =			\
	window.c				\
	window.h				\
	text-cursor-position-protocol.c		\
	text-cursor-position-client-protocol.h	\
	subsurface-protocol.c			\
	subsurface-client-protocol.h		\
	workspaces-protocol.c			\
	workspaces-client-protocol.h

libtoytoolkit_la_LIBADD =			\
	$(CLIENT_LIBS)				\
	$(CAIRO_EGL_LIBS)			\
	../shared/libshared-cairo.la -lrt -lm

flower_SOURCES = flower.c
flower_LDADD = libtoytoolkit.la

weston_screenshooter_SOURCES =			\
	screenshot.c				\
	screenshooter-protocol.c		\
	screenshooter-client-protocol.h
weston_screenshooter_LDADD = libtoytoolkit.la

weston_terminal_SOURCES = terminal.c
weston_terminal_LDADD = libtoytoolkit.la -lutil

image_SOURCES = image.c
image_LDADD = libtoytoolkit.la

cliptest_SOURCES = cliptest.c
cliptest_CPPFLAGS = $(AM_CPPFLAGS) $(PIXMAN_CFLAGS)
cliptest_LDADD = libtoytoolkit.la $(PIXMAN_LIBS)

dnd_SOURCES = dnd.c
dnd_LDADD = libtoytoolkit.la

smoke_SOURCES = smoke.c
smoke_LDADD = libtoytoolkit.la

resizor_SOURCES = resizor.c
resizor_LDADD = libtoytoolkit.la

eventdemo_SOURCES = eventdemo.c
eventdemo_LDADD = libtoytoolkit.la

clickdot_SOURCES = clickdot.c
clickdot_LDADD = libtoytoolkit.la

transformed_SOURCES = transformed.c
transformed_LDADD = libtoytoolkit.la

calibrator_SOURCES = calibrator.c ../shared/matrix.c ../shared/matrix.h
calibrator_LDADD = libtoytoolkit.la

if HAVE_PANGO
pango_programs = editor
editor_SOURCES = 				\
	editor.c				\
	text-protocol.c				\
	text-client-protocol.h
editor_LDADD = libtoytoolkit.la $(PANGO_LIBS)
editor_CPPFLAGS = $(AM_CPPFLAGS) $(PANGO_CFLAGS)
endif

keyboard = weston-keyboard
weston_keyboard_SOURCES = 			\
	keyboard.c				\
	desktop-shell-client-protocol.h		\
	desktop-shell-protocol.c		\
	input-method-protocol.c			\
	input-method-client-protocol.h
weston_keyboard_LDADD = libtoytoolkit.la

weston_simple_im_SOURCES = 			\
	weston-simple-im.c			\
	input-method-protocol.c			\
	input-method-client-protocol.h
weston_simple_im_LDADD = $(CLIENT_LIBS)

weston_info_SOURCES =				\
	weston-info.c				\
	../shared/os-compatibility.c		\
	../shared/os-compatibility.h
weston_info_LDADD = $(WESTON_INFO_LIBS)

weston_desktop_shell_SOURCES =			\
	desktop-shell.c				\
	desktop-shell-client-protocol.h		\
	desktop-shell-protocol.c
weston_desktop_shell_LDADD = libtoytoolkit.la

weston_tablet_shell_SOURCES =			\
	tablet-shell.c				\
	tablet-shell-client-protocol.h		\
	tablet-shell-protocol.c
weston_tablet_shell_LDADD = libtoytoolkit.la

BUILT_SOURCES =					\
	screenshooter-client-protocol.h		\
	screenshooter-protocol.c		\
	text-cursor-position-client-protocol.h	\
	text-cursor-position-protocol.c		\
	text-protocol.c				\
	text-client-protocol.h			\
	input-method-protocol.c			\
	input-method-client-protocol.h		\
	desktop-shell-client-protocol.h		\
	desktop-shell-protocol.c		\
	tablet-shell-client-protocol.h		\
	tablet-shell-protocol.c			\
	subsurface-client-protocol.h		\
	subsurface-protocol.c			\
	workspaces-client-protocol.h		\
	workspaces-protocol.c

CLEANFILES = $(BUILT_SOURCES)
endif

if BUILD_FULL_GL_CLIENTS
full_gl_client_programs = gears

gears_SOURCES = gears.c
gears_LDADD = libtoytoolkit.la

if HAVE_GLU
screensaver = weston-screensaver
weston_screensaver_SOURCES =			\
	wscreensaver.c				\
	wscreensaver.h				\
	desktop-shell-client-protocol.h		\
	desktop-shell-protocol.c		\
	wscreensaver-glue.c			\
	wscreensaver-glue.h			\
	glmatrix.c				\
	matrix3.xpm
weston_screensaver_LDADD = libtoytoolkit.la $(GLU_LIBS)
weston_screensaver_CFLAGS = $(GLU_CFLAGS)
endif

endif

@wayland_scanner_rules@

if HAVE_POPPLER
poppler_programs = view
view_SOURCES = view.c
view_LDADD = libtoytoolkit.la $(POPPLER_LIBS)
view_CPPFLAGS = $(AM_CPPFLAGS) $(POPPLER_CFLAGS)
endif
