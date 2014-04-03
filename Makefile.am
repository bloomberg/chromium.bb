bin_PROGRAMS =
noinst_PROGRAMS =
libexec_PROGRAMS =
moduledir = $(libdir)/weston
module_LTLIBRARIES =
noinst_LTLIBRARIES =
BUILT_SOURCES =

DISTCHECK_CONFIGURE_FLAGS = --disable-setuid-install

EXTRA_DIST = weston.ini.in

weston.ini : $(srcdir)/weston.ini.in
	$(AM_V_GEN)$(SED) \
		-e 's|@bindir[@]|$(bindir)|g' \
		-e 's|@abs_top_builddir[@]|$(abs_top_builddir)|g' \
		-e 's|@libexecdir[@]|$(libexecdir)|g' \
		$< > $@

all-local : weston.ini

AM_CFLAGS = $(GCC_CFLAGS)

AM_CPPFLAGS = 					\
	-I$(top_srcdir)/src			\
	-I$(top_builddir)/src			\
	-I$(top_builddir)/clients		\
	-I$(top_builddir)/tests			\
	-I$(top_srcdir)/shared			\
	-I$(top_builddir)/protocol		\
	-DDATADIR='"$(datadir)"'		\
	-DMODULEDIR='"$(moduledir)"'		\
	-DLIBEXECDIR='"$(libexecdir)"'		\
	-DBINDIR='"$(bindir)"'

CLEANFILES = weston.ini $(BUILT_SOURCES)

bin_PROGRAMS += weston

weston_LDFLAGS = -export-dynamic
weston_CPPFLAGS = $(AM_CPPFLAGS) -DIN_WESTON
weston_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS) $(LIBUNWIND_CFLAGS)
weston_LDADD = $(COMPOSITOR_LIBS) $(LIBUNWIND_LIBS) \
	$(DLOPEN_LIBS) -lm libshared.la

weston_SOURCES =					\
	src/git-version.h				\
	src/log.c					\
	src/compositor.c				\
	src/compositor.h				\
	src/input.c					\
	src/data-device.c				\
	src/screenshooter.c				\
	src/clipboard.c					\
	src/zoom.c					\
	src/text-backend.c				\
	src/bindings.c					\
	src/animation.c					\
	src/noop-renderer.c				\
	src/pixman-renderer.c				\
	src/pixman-renderer.h				\
	shared/matrix.c					\
	shared/matrix.h					\
	shared/zalloc.h					\
	src/weston-egl-ext.h

nodist_weston_SOURCES =					\
	protocol/screenshooter-protocol.c		\
	protocol/screenshooter-server-protocol.h	\
	protocol/text-cursor-position-protocol.c	\
	protocol/text-cursor-position-server-protocol.h	\
	protocol/text-protocol.c			\
	protocol/text-server-protocol.h			\
	protocol/input-method-protocol.c		\
	protocol/input-method-server-protocol.h		\
	protocol/workspaces-protocol.c			\
	protocol/workspaces-server-protocol.h		\
	protocol/scaler-protocol.c			\
	protocol/scaler-server-protocol.h

BUILT_SOURCES += $(nodist_weston_SOURCES)

# Track this dependency explicitly instead of using BUILT_SOURCES.  We
# add BUILT_SOURCES to CLEANFILES, but we want to keep git-version.h
# in case we're building from tarballs.

src/compositor.c : $(top_builddir)/src/git-version.h

noinst_LTLIBRARIES +=				\
	libsession-helper.la

libsession_helper_la_SOURCES =			\
	src/weston-launch.h			\
	src/launcher-util.c			\
	src/launcher-util.h
libsession_helper_la_CFLAGS = $(GCC_CFLAGS) $(LIBDRM_CFLAGS) $(PIXMAN_CFLAGS) $(COMPOSITOR_CFLAGS)
libsession_helper_la_LIBADD = $(LIBDRM_LIBS)

if ENABLE_DBUS
if HAVE_SYSTEMD_LOGIN
libsession_helper_la_SOURCES +=			\
	src/dbus.h				\
	src/dbus.c				\
	src/logind-util.h			\
	src/logind-util.c
libsession_helper_la_CFLAGS += $(SYSTEMD_LOGIN_CFLAGS) $(DBUS_CFLAGS)
libsession_helper_la_LIBADD += $(SYSTEMD_LOGIN_LIBS) $(DBUS_LIBS)
endif
endif

if HAVE_GIT_REPO
src/git-version.h : $(top_srcdir)/.git/logs/HEAD
	$(AM_V_GEN)echo "#define BUILD_ID \"$(shell git --git-dir=$(top_srcdir)/.git describe --always --dirty) $(shell git --git-dir=$(top_srcdir)/.git log -1 --format='%s (%ci)')\"" > $@
else
src/git-version.h :
	$(AM_V_GEN)echo "#define BUILD_ID \"unknown (not built from git or tarball)\"" > $@

endif

.FORCE :

if BUILD_WESTON_LAUNCH
bin_PROGRAMS += weston-launch
weston_launch_SOURCES = src/weston-launch.c src/weston-launch.h
weston_launch_CPPFLAGS = -DBINDIR='"$(bindir)"'
weston_launch_CFLAGS=				\
	$(GCC_CFLAGS)				\
	$(PAM_CFLAGS)				\
	$(SYSTEMD_LOGIN_CFLAGS)			\
	$(LIBDRM_CFLAGS)
weston_launch_LDADD = $(PAM_LIBS) $(SYSTEMD_LOGIN_LIBS) $(LIBDRM_LIBS)

if ENABLE_SETUID_INSTALL
install-exec-hook:
	chown root $(DESTDIR)$(bindir)/weston-launch
	chmod u+s $(DESTDIR)$(bindir)/weston-launch
endif

endif # BUILD_WESTON_LAUNCH

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = src/weston.pc

westonincludedir = $(includedir)/weston
westoninclude_HEADERS =				\
	src/version.h				\
	src/compositor.h			\
	shared/matrix.h				\
	shared/config-parser.h			\
	shared/zalloc.h

if ENABLE_EGL
module_LTLIBRARIES += gl-renderer.la
gl_renderer_la_LDFLAGS = -module -avoid-version
gl_renderer_la_LIBADD = $(COMPOSITOR_LIBS) $(EGL_LIBS)
gl_renderer_la_CFLAGS =				\
	$(COMPOSITOR_CFLAGS)			\
	$(EGL_CFLAGS)				\
	$(GCC_CFLAGS)
gl_renderer_la_SOURCES =			\
	src/gl-renderer.h			\
	src/gl-renderer.c			\
	src/vertex-clipping.c			\
	src/vertex-clipping.h
endif

if ENABLE_X11_COMPOSITOR
module_LTLIBRARIES += x11-backend.la
x11_backend_la_LDFLAGS = -module -avoid-version
x11_backend_la_LIBADD = $(COMPOSITOR_LIBS) $(X11_COMPOSITOR_LIBS) \
	libshared-cairo.la
x11_backend_la_CFLAGS =				\
	$(COMPOSITOR_CFLAGS)			\
	$(EGL_CFLAGS)				\
	$(PIXMAN_CFLAGS)			\
	$(CAIRO_CFLAGS)				\
	$(X11_COMPOSITOR_CFLAGS)		\
	$(GCC_CFLAGS)
x11_backend_la_SOURCES = src/compositor-x11.c
endif

INPUT_BACKEND_SOURCES = src/udev-input.h

if ENABLE_LIBINPUT_BACKEND
INPUT_BACKEND_LIBS = $(LIBINPUT_BACKEND_LIBS)
INPUT_BACKEND_SOURCES +=			\
	src/libinput-seat.c			\
	src/libinput-seat.h			\
	src/libinput-device.c			\
	src/libinput-device.h
else
INPUT_BACKEND_SOURCES +=			\
	src/filter.c				\
	src/filter.h				\
	src/udev-seat.c				\
	src/udev-seat.h				\
	src/evdev.c				\
	src/evdev.h				\
	src/evdev-touchpad.c
endif

if ENABLE_DRM_COMPOSITOR
module_LTLIBRARIES += drm-backend.la
drm_backend_la_LDFLAGS = -module -avoid-version
drm_backend_la_LIBADD =				\
	$(COMPOSITOR_LIBS)			\
	$(DRM_COMPOSITOR_LIBS)			\
	$(INPUT_BACKEND_LIBS)			\
	libshared.la -lrt			\
	libsession-helper.la
drm_backend_la_CFLAGS =				\
	$(COMPOSITOR_CFLAGS)			\
	$(EGL_CFLAGS)				\
	$(DRM_COMPOSITOR_CFLAGS)		\
	$(GCC_CFLAGS)
drm_backend_la_SOURCES =			\
	src/compositor-drm.c			\
	$(INPUT_BACKEND_SOURCES)		\
	src/libbacklight.c			\
	src/libbacklight.h

if ENABLE_VAAPI_RECORDER
drm_backend_la_SOURCES += src/vaapi-recorder.c src/vaapi-recorder.h
drm_backend_la_LIBADD += $(LIBVA_LIBS)
drm_backend_la_CFLAGS += $(LIBVA_CFLAGS)
endif
endif

if ENABLE_WAYLAND_COMPOSITOR
module_LTLIBRARIES += wayland-backend.la
wayland_backend_la_LDFLAGS = -module -avoid-version
wayland_backend_la_LIBADD =			\
	$(COMPOSITOR_LIBS)			\
	$(WAYLAND_COMPOSITOR_LIBS)		\
	libshared-cairo.la
wayland_backend_la_CFLAGS =			\
	$(COMPOSITOR_CFLAGS)			\
	$(EGL_CFLAGS)				\
	$(PIXMAN_CFLAGS)			\
	$(CAIRO_CFLAGS)				\
	$(WAYLAND_COMPOSITOR_CFLAGS)		\
	$(GCC_CFLAGS)
wayland_backend_la_SOURCES = src/compositor-wayland.c
endif

if ENABLE_RPI_COMPOSITOR
if INSTALL_RPI_COMPOSITOR
module_LTLIBRARIES += rpi-backend.la
else
noinst_LTLIBRARIES += rpi-backend.la
endif

rpi_backend_la_LDFLAGS = -module -avoid-version
rpi_backend_la_LIBADD = $(COMPOSITOR_LIBS)	\
	$(RPI_COMPOSITOR_LIBS)			\
	$(RPI_BCM_HOST_LIBS)			\
	$(INPUT_BACKEND_LIBS)			\
	libsession-helper.la			\
	libshared.la
rpi_backend_la_CFLAGS =				\
	$(GCC_CFLAGS)				\
	$(COMPOSITOR_CFLAGS)			\
	$(RPI_COMPOSITOR_CFLAGS)		\
	$(RPI_BCM_HOST_CFLAGS)
rpi_backend_la_SOURCES =			\
	src/compositor-rpi.c			\
	src/rpi-renderer.c			\
	src/rpi-renderer.h			\
	src/rpi-bcm-stubs.h			\
	$(INPUT_BACKEND_SOURCES)

if ENABLE_EGL
rpi_backend_la_LIBADD += $(EGL_LIBS)
rpi_backend_la_CFLAGS += $(EGL_CFLAGS)
endif

endif

if ENABLE_HEADLESS_COMPOSITOR
module_LTLIBRARIES += headless-backend.la
headless_backend_la_LDFLAGS = -module -avoid-version
headless_backend_la_LIBADD = $(COMPOSITOR_LIBS) libshared.la
headless_backend_la_CFLAGS = $(COMPOSITOR_CFLAGS) $(GCC_CFLAGS)
headless_backend_la_SOURCES = src/compositor-headless.c
endif

if ENABLE_FBDEV_COMPOSITOR
module_LTLIBRARIES += fbdev-backend.la
fbdev_backend_la_LDFLAGS = -module -avoid-version
fbdev_backend_la_LIBADD =			\
	$(COMPOSITOR_LIBS)			\
	$(FBDEV_COMPOSITOR_LIBS)		\
	$(INPUT_BACKEND_LIBS)			\
	libsession-helper.la			\
	libshared.la
fbdev_backend_la_CFLAGS =			\
	$(COMPOSITOR_CFLAGS)			\
	$(EGL_CFLAGS)				\
	$(FBDEV_COMPOSITOR_CFLAGS)		\
	$(PIXMAN_CFLAGS)			\
	$(GCC_CFLAGS)
fbdev_backend_la_SOURCES =			\
	src/compositor-fbdev.c			\
	$(INPUT_BACKEND_SOURCES)
endif

if ENABLE_RDP_COMPOSITOR
module_LTLIBRARIES += rdp-backend.la
rdp_backend_la_LDFLAGS = -module -avoid-version
rdp_backend_la_LIBADD = $(COMPOSITOR_LIBS) \
	$(RDP_COMPOSITOR_LIBS) \
	libshared.la
rdp_backend_la_CFLAGS =				\
	$(COMPOSITOR_CFLAGS)			\
	$(RDP_COMPOSITOR_CFLAGS)		\
	$(GCC_CFLAGS)
rdp_backend_la_SOURCES = src/compositor-rdp.c
endif

if HAVE_LCMS
module_LTLIBRARIES += cms-static.la
cms_static_la_LDFLAGS = -module -avoid-version
cms_static_la_LIBADD = $(COMPOSITOR_LIBS) $(LCMS_LIBS) libshared.la
cms_static_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS) $(LCMS_CFLAGS)
cms_static_la_SOURCES =				\
	src/cms-static.c				\
	src/cms-helper.c				\
	src/cms-helper.h
if ENABLE_COLORD
module_LTLIBRARIES += cms-colord.la
cms_colord_la_LDFLAGS = -module -avoid-version
cms_colord_la_LIBADD = $(COMPOSITOR_LIBS) $(COLORD_LIBS)
cms_colord_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS) $(COLORD_CFLAGS)
cms_colord_la_SOURCES =				\
	src/cms-colord.c			\
	src/cms-helper.c			\
	src/cms-helper.h
endif
endif

noinst_PROGRAMS += spring-tool
spring_tool_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)
spring_tool_LDADD = $(COMPOSITOR_LIBS) -lm
spring_tool_SOURCES =				\
	src/spring-tool.c			\
	src/animation.c				\
	shared/matrix.c				\
	shared/matrix.h				\
	src/compositor.h

if BUILD_CLIENTS

bin_PROGRAMS += weston-terminal weston-info

libexec_PROGRAMS +=				\
	weston-desktop-shell			\
	weston-screenshooter			\
	weston-keyboard				\
	weston-simple-im

demo_clients =					\
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
	weston-stacking				\
	weston-calibrator			\
	weston-scaler

if INSTALL_DEMO_CLIENTS
bin_PROGRAMS += $(demo_clients)
else
noinst_PROGRAMS += $(demo_clients)
endif


if BUILD_SIMPLE_CLIENTS
demo_clients +=					\
	weston-simple-shm			\
	weston-simple-touch			\
	weston-multi-resource

weston_simple_shm_SOURCES = clients/simple-shm.c
nodist_weston_simple_shm_SOURCES =		\
	protocol/xdg-shell-protocol.c		\
	protocol/xdg-shell-client-protocol.h	\
	protocol/fullscreen-shell-protocol.c	\
	protocol/fullscreen-shell-client-protocol.h
weston_simple_shm_CFLAGS = $(AM_CFLAGS) $(SIMPLE_CLIENT_CFLAGS)
weston_simple_shm_LDADD = $(SIMPLE_CLIENT_LIBS) libshared.la

weston_simple_touch_SOURCES = clients/simple-touch.c
weston_simple_touch_CFLAGS = $(AM_CFLAGS) $(SIMPLE_CLIENT_CFLAGS)
weston_simple_touch_LDADD = $(SIMPLE_CLIENT_LIBS) libshared.la

weston_multi_resource_SOURCES = clients/multi-resource.c
weston_multi_resource_CFLAGS = $(AM_CFLAGS) $(SIMPLE_CLIENT_CFLAGS)
weston_multi_resource_LDADD = $(SIMPLE_CLIENT_LIBS) libshared.la -lm
endif

if BUILD_SIMPLE_EGL_CLIENTS
demo_clients += weston-simple-egl
weston_simple_egl_SOURCES = clients/simple-egl.c
nodist_weston_simple_egl_SOURCES =		\
	protocol/xdg-shell-protocol.c		\
	protocol/xdg-shell-client-protocol.h
weston_simple_egl_CFLAGS = $(AM_CFLAGS) $(SIMPLE_EGL_CLIENT_CFLAGS)
weston_simple_egl_LDADD = $(SIMPLE_EGL_CLIENT_LIBS) -lm
endif

noinst_LTLIBRARIES += libtoytoolkit.la

libtoytoolkit_la_SOURCES =				\
	clients/window.c				\
	clients/window.h

nodist_libtoytoolkit_la_SOURCES =			\
	protocol/text-cursor-position-protocol.c	\
	protocol/text-cursor-position-client-protocol.h	\
	protocol/scaler-protocol.c			\
	protocol/scaler-client-protocol.h		\
	protocol/workspaces-protocol.c			\
	protocol/workspaces-client-protocol.h		\
	protocol/xdg-shell-protocol.c			\
	protocol/xdg-shell-client-protocol.h

BUILT_SOURCES += $(nodist_libtoytoolkit_la_SOURCES)


libtoytoolkit_la_LIBADD =			\
	$(CLIENT_LIBS)				\
	$(CAIRO_EGL_LIBS)			\
	libshared-cairo.la -lrt -lm
libtoytoolkit_la_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS) $(CAIRO_EGL_CFLAGS)

weston_flower_SOURCES = clients/flower.c
weston_flower_LDADD = libtoytoolkit.la
weston_flower_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_screenshooter_SOURCES =				\
	clients/screenshot.c
nodist_weston_screenshooter_SOURCES =			\
	protocol/screenshooter-protocol.c		\
	protocol/screenshooter-client-protocol.h
weston_screenshooter_LDADD = $(CLIENT_LIBS) libshared.la
weston_screenshooter_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_terminal_SOURCES = clients/terminal.c
weston_terminal_LDADD = libtoytoolkit.la -lutil
weston_terminal_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_image_SOURCES = clients/image.c
weston_image_LDADD = libtoytoolkit.la
weston_image_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_cliptest_SOURCES = clients/cliptest.c
weston_cliptest_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)
weston_cliptest_LDADD = libtoytoolkit.la

weston_dnd_SOURCES = clients/dnd.c
weston_dnd_LDADD = libtoytoolkit.la
weston_dnd_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_smoke_SOURCES = clients/smoke.c
weston_smoke_LDADD = libtoytoolkit.la
weston_smoke_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_resizor_SOURCES = clients/resizor.c
weston_resizor_LDADD = libtoytoolkit.la
weston_resizor_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_scaler_SOURCES = clients/scaler.c
weston_scaler_LDADD = libtoytoolkit.la
weston_scaler_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

if HAVE_CAIRO_GLESV2
demo_clients += weston-nested weston-nested-client

weston_nested_SOURCES = clients/nested.c
weston_nested_LDADD = libtoytoolkit.la $(SERVER_LIBS)
weston_nested_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_nested_client_SOURCES = clients/nested-client.c
weston_nested_client_LDADD = $(SIMPLE_EGL_CLIENT_LIBS) -lm
weston_nested_client_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)
endif

weston_eventdemo_SOURCES = clients/eventdemo.c
weston_eventdemo_LDADD = libtoytoolkit.la
weston_eventdemo_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_clickdot_SOURCES = clients/clickdot.c
weston_clickdot_LDADD = libtoytoolkit.la
weston_clickdot_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_transformed_SOURCES = clients/transformed.c
weston_transformed_LDADD = libtoytoolkit.la
weston_transformed_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_fullscreen_SOURCES = clients/fullscreen.c
nodist_weston_fullscreen_SOURCES =			\
	protocol/fullscreen-shell-protocol.c		\
	protocol/fullscreen-shell-client-protocol.h
weston_fullscreen_LDADD = libtoytoolkit.la
weston_fullscreen_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_stacking_SOURCES = clients/stacking.c
weston_stacking_LDADD = libtoytoolkit.la
weston_stacking_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_calibrator_SOURCES = clients/calibrator.c	\
	shared/matrix.c					\
	shared/matrix.h
weston_calibrator_LDADD = libtoytoolkit.la
weston_calibrator_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

if BUILD_SUBSURFACES_CLIENT
demo_clients += weston-subsurfaces
weston_subsurfaces_SOURCES = clients/subsurfaces.c
weston_subsurfaces_CFLAGS =			\
	$(AM_CFLAGS)				\
	$(SIMPLE_EGL_CLIENT_CFLAGS)		\
	$(CLIENT_CFLAGS)
weston_subsurfaces_LDADD = libtoytoolkit.la $(SIMPLE_EGL_CLIENT_LIBS) -lm
endif

if HAVE_PANGO
demo_clients += weston-editor
weston_editor_SOURCES = clients/editor.c
nodist_weston_editor_SOURCES =			\
	protocol/text-protocol.c		\
	protocol/text-client-protocol.h
weston_editor_LDADD = libtoytoolkit.la $(PANGO_LIBS)
weston_editor_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS) $(PANGO_CFLAGS)
endif

weston_keyboard_SOURCES = clients/keyboard.c
nodist_weston_keyboard_SOURCES =			\
	protocol/desktop-shell-client-protocol.h	\
	protocol/desktop-shell-protocol.c		\
	protocol/input-method-protocol.c		\
	protocol/input-method-client-protocol.h
weston_keyboard_LDADD = libtoytoolkit.la
weston_keyboard_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_simple_im_SOURCES = clients/weston-simple-im.c
nodist_weston_simple_im_SOURCES =		\
	protocol/input-method-protocol.c	\
	protocol/input-method-client-protocol.h
weston_simple_im_LDADD = $(CLIENT_LIBS)
weston_simple_im_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_info_SOURCES = clients/weston-info.c
weston_info_LDADD = $(WESTON_INFO_LIBS) libshared.la
weston_info_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

weston_desktop_shell_SOURCES = clients/desktop-shell.c
nodist_weston_desktop_shell_SOURCES =			\
	protocol/desktop-shell-client-protocol.h	\
	protocol/desktop-shell-protocol.c
weston_desktop_shell_LDADD = libtoytoolkit.la
weston_desktop_shell_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

if BUILD_FULL_GL_CLIENTS
demo_clients += weston-gears
weston_gears_SOURCES = clients/gears.c
weston_gears_LDADD = libtoytoolkit.la
weston_gears_CFLAGS = $(AM_CFLAGS) $(CLIENT_CFLAGS)

if HAVE_GLU
libexec_PROGRAMS += weston-screensaver
weston_screensaver_SOURCES =				\
	clients/wscreensaver.c				\
	clients/wscreensaver.h				\
	clients/wscreensaver-glue.c			\
	clients/wscreensaver-glue.h			\
	clients/glmatrix.c				\
	clients/matrix3.xpm
nodist_weston_screensaver_SOURCES =			\
	protocol/desktop-shell-client-protocol.h	\
	protocol/desktop-shell-protocol.c

weston_screensaver_LDADD = libtoytoolkit.la $(GLU_LIBS)
weston_screensaver_CFLAGS = $(AM_CFLAGS) $(GLU_CFLAGS) $(CLIENT_CFLAGS)

endif

endif

endif

BUILT_SOURCES +=					\
	protocol/screenshooter-protocol.c		\
	protocol/screenshooter-client-protocol.h	\
	protocol/text-cursor-position-client-protocol.h	\
	protocol/text-cursor-position-protocol.c	\
	protocol/text-protocol.c			\
	protocol/text-client-protocol.h			\
	protocol/input-method-protocol.c		\
	protocol/input-method-client-protocol.h		\
	protocol/desktop-shell-client-protocol.h	\
	protocol/desktop-shell-protocol.c		\
	protocol/scaler-client-protocol.h		\
	protocol/scaler-protocol.c			\
	protocol/workspaces-client-protocol.h		\
	protocol/workspaces-protocol.c			\
	protocol/fullscreen-shell-protocol.c		\
	protocol/fullscreen-shell-client-protocol.h	\
	protocol/xdg-shell-protocol.c			\
	protocol/xdg-shell-client-protocol.h


westondatadir = $(datadir)/weston
dist_westondata_DATA =				\
	data/wayland.svg			\
	data/wayland.png			\
	data/pattern.png			\
	data/terminal.png			\
	data/border.png				\
	data/icon_window.png			\
	data/sign_close.png			\
	data/sign_maximize.png			\
	data/sign_minimize.png


if BUILD_WCAP_TOOLS
bin_PROGRAMS += wcap-decode

wcap_decode_SOURCES =				\
	wcap/main.c				\
	wcap/wcap-decode.c			\
	wcap/wcap-decode.h

wcap_decode_CFLAGS = $(GCC_CFLAGS) $(WCAP_CFLAGS)
wcap_decode_LDADD = $(WCAP_LIBS)
endif


if ENABLE_DESKTOP_SHELL

module_LTLIBRARIES += desktop-shell.la

desktop_shell_la_CPPFLAGS =			\
	-I$(top_builddir)/protocol		\
	-I$(top_srcdir)/shared			\
	-I$(top_srcdir)/src			\
	-I$(top_builddir)/src			\
	-I$(top_builddir)/desktop-shell		\
	-DDATADIR='"$(datadir)"'		\
	-DMODULEDIR='"$(moduledir)"'		\
	-DLIBEXECDIR='"$(libexecdir)"'		\
	-DIN_WESTON

desktop_shell_la_LDFLAGS = -module -avoid-version
desktop_shell_la_LIBADD = $(COMPOSITOR_LIBS) libshared.la
desktop_shell_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)
desktop_shell_la_SOURCES =				\
	desktop-shell/shell.h				\
	desktop-shell/shell.c				\
	desktop-shell/exposay.c				\
	desktop-shell/input-panel.c
nodist_desktop_shell_la_SOURCES =			\
	protocol/desktop-shell-protocol.c		\
	protocol/desktop-shell-server-protocol.h	\
	protocol/xdg-shell-protocol.c			\
	protocol/xdg-shell-server-protocol.h

BUILT_SOURCES += $(nodist_desktop_shell_la_SOURCES)
endif

if ENABLE_FULLSCREEN_SHELL

module_LTLIBRARIES += fullscreen-shell.la

fullscreen_shell_la_CPPFLAGS =			\
	-I$(top_builddir)/protocol		\
	-I$(top_srcdir)/shared			\
	-I$(top_srcdir)/src			\
	-I$(top_builddir)/src			\
	-DIN_WESTON

fullscreen_shell_la_LDFLAGS = -module -avoid-version
fullscreen_shell_la_LIBADD = $(COMPOSITOR_LIBS)
fullscreen_shell_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)
fullscreen_shell_la_SOURCES =			\
	fullscreen-shell/fullscreen-shell.c
nodist_fullscreen_shell_la_SOURCES =			\
	protocol/fullscreen-shell-protocol.c		\
	protocol/fullscreen-shell-server-protocol.h

BUILT_SOURCES += $(nodist_fullscreen_shell_la_SOURCES)
endif

if ENABLE_XWAYLAND

module_LTLIBRARIES += xwayland.la

xwayland_la_CPPFLAGS =				\
	-I$(top_builddir)/protocol		\
	-I$(top_srcdir)/shared			\
	-I$(top_srcdir)/src			\
	-I$(top_builddir)/src			\
	-I$(top_builddir)/xwayland		\
	-DDATADIR='"$(datadir)"'		\
	-DMODULEDIR='"$(moduledir)"'		\
	-DLIBEXECDIR='"$(libexecdir)"'		\
	-DXSERVER_PATH='"@XSERVER_PATH@"'

xwayland_la_LDFLAGS = -module -avoid-version
xwayland_la_LIBADD =			\
	$(XWAYLAND_LIBS)		\
	$(top_builddir)/libshared-cairo.la
xwayland_la_CFLAGS =				\
	$(GCC_CFLAGS)				\
	$(COMPOSITOR_CFLAGS)			\
	$(PIXMAN_CFLAGS)			\
	$(CAIRO_CFLAGS)
xwayland_la_SOURCES =				\
	xwayland/xwayland.h			\
	xwayland/window-manager.c		\
	xwayland/selection.c			\
	xwayland/dnd.c				\
	xwayland/launcher.c			\
	xwayland/hash.c				\
	xwayland/hash.h
nodist_xwayland_la_SOURCES =			\
	protocol/xserver-protocol.c		\
	protocol/xserver-server-protocol.h

BUILT_SOURCES += $(nodist_xwayland_la_SOURCES)
endif


#
# Shared utilities
#

noinst_LTLIBRARIES += libshared.la libshared-cairo.la

libshared_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)

libshared_la_SOURCES =				\
	shared/config-parser.c			\
	shared/option-parser.c			\
	shared/config-parser.h			\
	shared/os-compatibility.c		\
	shared/os-compatibility.h

libshared_cairo_la_CFLAGS =			\
	-DDATADIR='"$(datadir)"'		\
	$(GCC_CFLAGS)				\
	$(COMPOSITOR_CFLAGS)			\
	$(PIXMAN_CFLAGS)			\
	$(CAIRO_CFLAGS)				\
	$(PNG_CFLAGS)				\
	$(WEBP_CFLAGS)

libshared_cairo_la_LIBADD =			\
	$(PIXMAN_LIBS)				\
	$(CAIRO_LIBS)				\
	$(PNG_LIBS)				\
	$(WEBP_LIBS)				\
	$(JPEG_LIBS)

libshared_cairo_la_SOURCES =			\
	$(libshared_la_SOURCES)			\
	shared/image-loader.c			\
	shared/image-loader.h			\
	shared/cairo-util.c			\
	shared/frame.c				\
	shared/cairo-util.h


#
# tests subdirectory
#

TESTS = $(shared_tests) $(module_tests) $(weston_tests)

shared_tests =					\
	config-parser.test			\
	vertex-clip.test

module_tests =					\
	surface-test.la				\
	surface-global-test.la

weston_tests =					\
	bad_buffer.weston			\
	keyboard.weston				\
	event.weston				\
	button.weston				\
	text.weston				\
	subsurface.weston


AM_TESTS_ENVIRONMENT = \
	abs_builddir='$(abs_builddir)'; export abs_builddir;

TEST_EXTENSIONS = .la .weston
LA_LOG_COMPILER = $(srcdir)/tests/weston-tests-env
WESTON_LOG_COMPILER = $(srcdir)/tests/weston-tests-env

clean-local:
	-rm -rf tests/logs

# To remove when automake 1.11 support is dropped
export abs_builddir

noinst_LTLIBRARIES +=			\
	weston-test.la			\
	$(module_tests)			\
	libtest-runner.la		\
	libtest-client.la

noinst_PROGRAMS +=			\
	$(setbacklight)			\
	$(shared_tests)			\
	$(weston_tests)			\
	matrix-test

test_module_ldflags = \
	-module -avoid-version -rpath $(libdir) $(COMPOSITOR_LIBS)

surface_global_test_la_SOURCES = tests/surface-global-test.c
surface_global_test_la_LDFLAGS = $(test_module_ldflags)
surface_global_test_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)

surface_test_la_SOURCES = tests/surface-test.c
surface_test_la_LDFLAGS = $(test_module_ldflags)
surface_test_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)

weston_test_la_LIBADD = $(COMPOSITOR_LIBS) libshared.la
weston_test_la_LDFLAGS = $(test_module_ldflags)
weston_test_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)
weston_test_la_SOURCES = tests/weston-test.c
nodist_weston_test_la_SOURCES =			\
	protocol/wayland-test-protocol.c	\
	protocol/wayland-test-server-protocol.h

if ENABLE_EGL
weston_test_la_CFLAGS += $(EGL_TESTS_CFLAGS)
weston_test_la_LDFLAGS += $(EGL_TESTS_LIBS)
endif

libtest_runner_la_SOURCES =			\
	tests/weston-test-runner.c		\
	tests/weston-test-runner.h
libtest_runner_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)

config_parser_test_SOURCES = tests/config-parser-test.c
config_parser_test_LDADD = libshared.la libtest-runner.la $(COMPOSITOR_LIBS)

vertex_clip_test_SOURCES =			\
	tests/vertex-clip-test.c		\
	src/vertex-clipping.c			\
	src/vertex-clipping.h
vertex_clip_test_LDADD = libtest-runner.la -lm -lrt

libtest_client_la_SOURCES =			\
	tests/weston-test-client-helper.c	\
	tests/weston-test-client-helper.h
nodist_libtest_client_la_SOURCES =		\
	protocol/wayland-test-protocol.c	\
	protocol/wayland-test-client-protocol.h
libtest_client_la_CFLAGS = $(AM_CFLAGS) $(TEST_CLIENT_CFLAGS)
libtest_client_la_LIBADD = $(TEST_CLIENT_LIBS) libshared.la libtest-runner.la

bad_buffer_weston_SOURCES = tests/bad-buffer-test.c
bad_buffer_weston_CFLAGS = $(AM_CFLAGS) $(TEST_CLIENT_CFLAGS)
bad_buffer_weston_LDADD = libtest-client.la

keyboard_weston_SOURCES = tests/keyboard-test.c
keyboard_weston_CFLAGS = $(AM_CFLAGS) $(TEST_CLIENT_CFLAGS)
keyboard_weston_LDADD = libtest-client.la

event_weston_SOURCES = tests/event-test.c
event_weston_CFLAGS = $(AM_CFLAGS) $(TEST_CLIENT_CFLAGS)
event_weston_LDADD = libtest-client.la

button_weston_SOURCES = tests/button-test.c
button_weston_CFLAGS = $(AM_CFLAGS) $(TEST_CLIENT_CFLAGS)
button_weston_LDADD = libtest-client.la

text_weston_SOURCES = tests/text-test.c
nodist_text_weston_SOURCES =			\
	protocol/text-protocol.c		\
	protocol/text-client-protocol.h
text_weston_CFLAGS = $(AM_CFLAGS) $(TEST_CLIENT_CFLAGS)
text_weston_LDADD = libtest-client.la

subsurface_weston_SOURCES = tests/subsurface-test.c
subsurface_weston_CFLAGS = $(AM_CFLAGS) $(TEST_CLIENT_CFLAGS)
subsurface_weston_LDADD = libtest-client.la

if ENABLE_EGL
weston_tests += buffer-count.weston
buffer_count_weston_SOURCES = tests/buffer-count-test.c
buffer_count_weston_CFLAGS = $(GCC_CFLAGS) $(EGL_TESTS_CFLAGS)
buffer_count_weston_LDADD = libtest-client.la $(EGL_TESTS_LIBS)
endif

if ENABLE_XWAYLAND_TEST
weston_tests +=	xwayland.weston
xwayland_weston_SOURCES = tests/xwayland-test.c
xwayland_weston_CFLAGS = $(GCC_CFLAGS) $(XWAYLAND_TEST_CFLAGS)
xwayland_weston_LDADD = libtest-client.la $(XWAYLAND_TEST_LIBS)
endif

matrix_test_SOURCES =				\
	tests/matrix-test.c			\
	shared/matrix.c				\
	shared/matrix.h
matrix_test_CPPFLAGS = -DUNIT_TEST
matrix_test_LDADD = -lm -lrt

if BUILD_SETBACKLIGHT
noinst_PROGRAMS += setbacklight
setbacklight_SOURCES =				\
	tests/setbacklight.c			\
	src/libbacklight.c			\
	src/libbacklight.h
setbacklight_CFLAGS = $(AM_CFLAGS) $(SETBACKLIGHT_CFLAGS)
setbacklight_LDADD = $(SETBACKLIGHT_LIBS)
endif

EXTRA_DIST += tests/weston-tests-env

BUILT_SOURCES +=				\
	protocol/wayland-test-protocol.c	\
	protocol/wayland-test-server-protocol.h	\
	protocol/wayland-test-client-protocol.h	\
	protocol/text-protocol.c		\
	protocol/text-client-protocol.h

EXTRA_DIST +=					\
	protocol/desktop-shell.xml		\
	protocol/screenshooter.xml		\
	protocol/xserver.xml			\
	protocol/text.xml			\
	protocol/input-method.xml		\
	protocol/workspaces.xml			\
	protocol/text-cursor-position.xml	\
	protocol/wayland-test.xml		\
	protocol/xdg-shell.xml			\
	protocol/fullscreen-shell.xml		\
	protocol/scaler.xml

man_MANS = weston.1 weston.ini.5

if ENABLE_DRM_COMPOSITOR
man_MANS += weston-drm.7
endif

MAN_SUBSTS =								\
	-e 's|__weston_native_backend__|$(WESTON_NATIVE_BACKEND)|g'	\
	-e 's|__weston_modules_dir__|$(pkglibdir)|g'			\
	-e 's|__weston_shell_client__|$(WESTON_SHELL_CLIENT)|g'		\
	-e 's|__version__|$(PACKAGE_VERSION)|g'

SUFFIXES = .1 .5 .7 .man

%.1 %.5 %.7 : man/%.man
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

EXTRA_DIST +=					\
	man/weston.man				\
	man/weston-drm.man			\
	man/weston.ini.man

CLEANFILES += $(man_MANS)


protocol/%-protocol.c : $(top_srcdir)/protocol/%.xml
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(wayland_scanner) code < $< > $@

protocol/%-server-protocol.h : $(top_srcdir)/protocol/%.xml
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(wayland_scanner) server-header < $< > $@

protocol/%-client-protocol.h : $(top_srcdir)/protocol/%.xml
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(wayland_scanner) client-header < $< > $@
