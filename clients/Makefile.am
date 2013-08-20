bin_PROGRAMS =					\
	weston-info				\
	$(terminal)

if ENABLE_DEMO_CLIENTS
bin_PROGRAMS +=					\
	$(clients_programs)			\
	$(pango_programs)			\
	$(poppler_programs)			\
	$(simple_clients_programs)		\
	$(simple_egl_clients_programs)
else
noinst_PROGRAMS =				\
	$(clients_programs)			\
	$(pango_programs)			\
	$(poppler_programs)			\
	$(simple_clients_programs)		\
	$(simple_egl_clients_programs)
endif

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
	weston-simple-shm			\
	weston-simple-touch

weston_simple_shm_SOURCES = simple-shm.c 	\
	../shared/os-compatibility.c		\
	../shared/os-compatibility.h
weston_simple_shm_CPPFLAGS = $(SIMPLE_CLIENT_CFLAGS)
weston_simple_shm_LDADD = $(SIMPLE_CLIENT_LIBS)

weston_simple_touch_SOURCES = simple-touch.c	\
	../shared/os-compatibility.c		\
	../shared/os-compatibility.h
weston_simple_touch_CPPFLAGS = $(SIMPLE_CLIENT_CFLAGS)
weston_simple_touch_LDADD = $(SIMPLE_CLIENT_LIBS)
endif

if BUILD_SIMPLE_EGL_CLIENTS
simple_egl_clients_programs =			\
	weston-simple-egl

weston_simple_egl_SOURCES = simple-egl.c
weston_simple_egl_CPPFLAGS = $(SIMPLE_EGL_CLIENT_CFLAGS)
weston_simple_egl_LDADD = $(SIMPLE_EGL_CLIENT_LIBS) -lm
endif

if BUILD_CLIENTS
terminal = weston-terminal

clients_programs =				\
	weston-flower				\
	weston-image				\
	weston-cliptest				\
	weston-dnd				\
	weston-smoke				\
	weston-resizor				\
	weston-eventdemo			\
	weston-clickdot				\
	weston-transformed			\
	weston-fullscreen			\
	weston-calibrator			\
	$(subsurfaces)				\
	$(full_gl_client_programs)		\
	$(cairo_glesv2_programs)

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

weston_flower_SOURCES = flower.c
weston_flower_LDADD = libtoytoolkit.la

weston_screenshooter_SOURCES =			\
	screenshot.c				\
	screenshooter-protocol.c		\
	screenshooter-client-protocol.h		\
	../shared/os-compatibility.c		\
	../shared/os-compatibility.h
weston_screenshooter_LDADD = $(CLIENT_LIBS)

weston_terminal_SOURCES = terminal.c
weston_terminal_LDADD = libtoytoolkit.la -lutil

weston_image_SOURCES = image.c
weston_image_LDADD = libtoytoolkit.la

weston_cliptest_SOURCES = cliptest.c
weston_cliptest_CPPFLAGS = $(AM_CPPFLAGS) $(PIXMAN_CFLAGS)
weston_cliptest_LDADD = libtoytoolkit.la $(PIXMAN_LIBS)

weston_dnd_SOURCES = dnd.c
weston_dnd_LDADD = libtoytoolkit.la

weston_smoke_SOURCES = smoke.c
weston_smoke_LDADD = libtoytoolkit.la

weston_resizor_SOURCES = resizor.c
weston_resizor_LDADD = libtoytoolkit.la

if HAVE_CAIRO_GLESV2
cairo_glesv2_programs = weston-nested weston-nested-client

weston_nested_SOURCES = nested.c
weston_nested_LDADD = libtoytoolkit.la $(SERVER_LIBS)

weston_nested_client_SOURCES = nested-client.c
weston_nested_client_LDADD = $(SIMPLE_EGL_CLIENT_LIBS) -lm
endif

weston_eventdemo_SOURCES = eventdemo.c
weston_eventdemo_LDADD = libtoytoolkit.la

weston_clickdot_SOURCES = clickdot.c
weston_clickdot_LDADD = libtoytoolkit.la

weston_transformed_SOURCES = transformed.c
weston_transformed_LDADD = libtoytoolkit.la

weston_fullscreen_SOURCES = fullscreen.c
weston_fullscreen_LDADD = libtoytoolkit.la

weston_calibrator_SOURCES = calibrator.c	\
	../shared/matrix.c			\
	../shared/matrix.h
weston_calibrator_LDADD = libtoytoolkit.la

if BUILD_SUBSURFACES_CLIENT
subsurfaces = weston-subsurfaces
weston_subsurfaces_SOURCES = subsurfaces.c
weston_subsurfaces_CPPFLAGS = $(AM_CPPFLAGS) $(SIMPLE_EGL_CLIENT_CFLAGS)
weston_subsurfaces_LDADD = libtoytoolkit.la $(SIMPLE_EGL_CLIENT_LIBS) -lm
endif

if HAVE_PANGO
pango_programs = weston-editor
weston_editor_SOURCES = 			\
	editor.c				\
	text-protocol.c				\
	text-client-protocol.h
weston_editor_LDADD = libtoytoolkit.la $(PANGO_LIBS)
weston_editor_CPPFLAGS = $(AM_CPPFLAGS) $(PANGO_CFLAGS)
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
full_gl_client_programs = weston-gears

weston_gears_SOURCES = gears.c
weston_gears_LDADD = libtoytoolkit.la

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

wayland_protocoldir = $(top_srcdir)/protocol
include $(top_srcdir)/wayland-scanner.mk

if HAVE_POPPLER
poppler_programs = weston-view
weston_view_SOURCES = view.c
weston_view_LDADD = libtoytoolkit.la $(POPPLER_LIBS)
weston_view_CPPFLAGS = $(AM_CPPFLAGS) $(POPPLER_CFLAGS)
endif
