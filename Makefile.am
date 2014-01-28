bin_PROGRAMS =
noinst_PROGRAMS =
moduledir = $(libdir)/weston
module_LTLIBRARIES =
noinst_LTLIBRARIES =
BUILT_SOURCES =

SUBDIRS =					\
	shared					\
	clients					\
	tests

DISTCHECK_CONFIGURE_FLAGS = --disable-setuid-install

EXTRA_DIST = weston.ini.in wayland-scanner.mk

weston.ini : $(srcdir)/weston.ini.in
	$(AM_V_GEN)$(SED) \
		-e 's|@bindir[@]|$(bindir)|g' \
		-e 's|@abs_top_builddir[@]|$(abs_top_builddir)|g' \
		-e 's|@libexecdir[@]|$(libexecdir)|g' \
		$< > $@

all-local : weston.ini validate-protocol-xml

AM_CPPFLAGS = 					\
	-I$(top_builddir)/src			\
	-I$(top_srcdir)/shared			\
	-DDATADIR='"$(datadir)"'		\
	-DMODULEDIR='"$(moduledir)"'		\
	-DLIBEXECDIR='"$(libexecdir)"'

CLEANFILES = weston.ini $(BUILT_SOURCES)

bin_PROGRAMS += weston

weston_LDFLAGS = -export-dynamic
weston_CPPFLAGS = $(AM_CPPFLAGS) -DIN_WESTON
weston_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS) $(LIBUNWIND_CFLAGS)
weston_LDADD = $(COMPOSITOR_LIBS) $(LIBUNWIND_LIBS) \
	$(DLOPEN_LIBS) -lm shared/libshared.la

weston_SOURCES =					\
	src/git-version.h				\
	src/log.c					\
	src/compositor.c				\
	src/compositor.h				\
	src/input.c					\
	src/data-device.c				\
	src/filter.c					\
	src/filter.h					\
	src/screenshooter.c				\
	src/screenshooter-protocol.c			\
	src/screenshooter-server-protocol.h		\
	src/clipboard.c					\
	src/text-cursor-position-protocol.c		\
	src/text-cursor-position-server-protocol.h	\
	src/zoom.c					\
	src/text-backend.c				\
	src/text-protocol.c				\
	src/text-server-protocol.h			\
	src/input-method-protocol.c			\
	src/input-method-server-protocol.h		\
	src/workspaces-protocol.c			\
	src/workspaces-server-protocol.h		\
	src/scaler-protocol.c				\
	src/scaler-server-protocol.h			\
	src/bindings.c					\
	src/animation.c					\
	src/noop-renderer.c				\
	src/pixman-renderer.c				\
	src/pixman-renderer.h				\
	shared/matrix.c					\
	shared/matrix.h					\
	shared/zalloc.h					\
	src/weston-egl-ext.h

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

src/git-version.h : .FORCE
	$(AM_V_GEN)(echo "#define BUILD_ID \"$(shell git --git-dir=$(top_srcdir)/.git describe --always --dirty) $(shell git --git-dir=$(top_srcdir)/.git log -1 --format='%s (%ci)')\"" > $@-new; \
	cmp -s $@ $@-new || cp $@-new $@; \
	rm $@-new)

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
	shared/libshared-cairo.la
x11_backend_la_CFLAGS =				\
	$(COMPOSITOR_CFLAGS)			\
	$(EGL_CFLAGS)				\
	$(PIXMAN_CFLAGS)			\
	$(CAIRO_CFLAGS)				\
	$(X11_COMPOSITOR_CFLAGS)		\
	$(GCC_CFLAGS)
x11_backend_la_SOURCES = src/compositor-x11.c
endif

if ENABLE_DRM_COMPOSITOR
module_LTLIBRARIES += drm-backend.la
drm_backend_la_LDFLAGS = -module -avoid-version
drm_backend_la_LIBADD =				\
	$(COMPOSITOR_LIBS)			\
	$(DRM_COMPOSITOR_LIBS)			\
	shared/libshared.la -lrt		\
	libsession-helper.la
drm_backend_la_CFLAGS =				\
	$(COMPOSITOR_CFLAGS)			\
	$(EGL_CFLAGS)				\
	$(DRM_COMPOSITOR_CFLAGS)		\
	$(GCC_CFLAGS)
drm_backend_la_SOURCES =			\
	src/compositor-drm.c			\
	src/udev-seat.c				\
	src/udev-seat.h				\
	src/evdev.c				\
	src/evdev.h				\
	src/evdev-touchpad.c			\
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
	shared/libshared-cairo.la
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
	libsession-helper.la			\
	shared/libshared.la
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
	src/udev-seat.c				\
	src/udev-seat.h				\
	src/evdev.c				\
	src/evdev.h				\
	src/evdev-touchpad.c

if ENABLE_EGL
rpi_backend_la_LIBADD += $(EGL_LIBS)
rpi_backend_la_CFLAGS += $(EGL_CFLAGS)
endif

endif

if ENABLE_HEADLESS_COMPOSITOR
module_LTLIBRARIES += headless-backend.la
headless_backend_la_LDFLAGS = -module -avoid-version
headless_backend_la_LIBADD = $(COMPOSITOR_LIBS) \
	shared/libshared.la
headless_backend_la_CFLAGS =			\
	$(COMPOSITOR_CFLAGS)			\
	$(GCC_CFLAGS)
headless_backend_la_SOURCES = src/compositor-headless.c
endif

if ENABLE_FBDEV_COMPOSITOR
module_LTLIBRARIES += fbdev-backend.la
fbdev_backend_la_LDFLAGS = -module -avoid-version
fbdev_backend_la_LIBADD =			\
	$(COMPOSITOR_LIBS)			\
	$(FBDEV_COMPOSITOR_LIBS)		\
	libsession-helper.la			\
	shared/libshared.la
fbdev_backend_la_CFLAGS =			\
	$(COMPOSITOR_CFLAGS)			\
	$(EGL_CFLAGS)				\
	$(FBDEV_COMPOSITOR_CFLAGS)		\
	$(PIXMAN_CFLAGS)			\
	$(GCC_CFLAGS)
fbdev_backend_la_SOURCES =			\
	src/compositor-fbdev.c			\
	src/udev-seat.c				\
	src/udev-seat.h				\
	src/evdev.c				\
	src/evdev.h				\
	src/evdev-touchpad.c
endif

if ENABLE_RDP_COMPOSITOR
module_LTLIBRARIES += rdp-backend.la
rdp_backend_la_LDFLAGS = -module -avoid-version
rdp_backend_la_LIBADD = $(COMPOSITOR_LIBS) \
	$(RDP_COMPOSITOR_LIBS) \
	shared/libshared.la
rdp_backend_la_CFLAGS =				\
	$(COMPOSITOR_CFLAGS)			\
	$(RDP_COMPOSITOR_CFLAGS)		\
	$(GCC_CFLAGS)
rdp_backend_la_SOURCES = src/compositor-rdp.c
endif

if HAVE_LCMS
module_LTLIBRARIES += cms-static.la
cms_static_la_LDFLAGS = -module -avoid-version
cms_static_la_LIBADD = $(COMPOSITOR_LIBS) $(LCMS_LIBS) shared/libshared.la
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
	src/cms-colord.c				\
	src/cms-helper.c				\
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

BUILT_SOURCES +=					\
	src/screenshooter-server-protocol.h		\
	src/screenshooter-protocol.c			\
	src/text-cursor-position-server-protocol.h	\
	src/text-cursor-position-protocol.c		\
	src/text-protocol.c				\
	src/text-server-protocol.h			\
	src/input-method-protocol.c			\
	src/input-method-server-protocol.h		\
	src/workspaces-server-protocol.h		\
	src/workspaces-protocol.c			\
	src/scaler-server-protocol.h			\
	src/scaler-protocol.c				\
	src/git-version.h

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
	-I$(top_srcdir)/shared			\
	-I$(top_srcdir)/src			\
	-I$(top_builddir)/src			\
	-I$(top_builddir)/desktop-shell		\
	-DDATADIR='"$(datadir)"'		\
	-DMODULEDIR='"$(moduledir)"'		\
	-DLIBEXECDIR='"$(libexecdir)"'		\
	-DIN_WESTON

desktop_shell_la_LDFLAGS = -module -avoid-version
desktop_shell_la_LIBADD = $(COMPOSITOR_LIBS)	\
	shared/libshared.la
desktop_shell_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)
desktop_shell_la_SOURCES =				\
	desktop-shell/shell.h				\
	desktop-shell/shell.c				\
	desktop-shell/exposay.c				\
	desktop-shell/input-panel.c			\
	desktop-shell/desktop-shell-protocol.c		\
	desktop-shell/desktop-shell-server-protocol.h	\
	desktop-shell/xdg-shell-protocol.c		\
	desktop-shell/xdg-shell-server-protocol.h

BUILT_SOURCES +=					\
	desktop-shell/desktop-shell-protocol.c		\
	desktop-shell/desktop-shell-server-protocol.h	\
	desktop-shell/xdg-shell-protocol.c		\
	desktop-shell/xdg-shell-server-protocol.h
endif


if ENABLE_XWAYLAND

module_LTLIBRARIES += xwayland.la

xwayland_la_CPPFLAGS =				\
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
	$(top_builddir)/shared/libshared-cairo.la
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
	xwayland/xserver-protocol.c		\
	xwayland/xserver-server-protocol.h	\
	xwayland/hash.c				\
	xwayland/hash.h

BUILT_SOURCES +=				\
	xwayland/xserver-protocol.c		\
	xwayland/xserver-server-protocol.h
endif


protocol_sources =				\
	protocol/desktop-shell.xml		\
	protocol/screenshooter.xml		\
	protocol/xserver.xml			\
	protocol/text.xml			\
	protocol/input-method.xml		\
	protocol/workspaces.xml			\
	protocol/text-cursor-position.xml	\
	protocol/wayland-test.xml		\
	protocol/xdg-shell.xml			\
	protocol/scaler.xml

if HAVE_XMLLINT
.PHONY : validate-protocol-xml

.%.xml.valid : $(top_srcdir)/protocol/%.xml
	$(AM_V_GEN)$(XMLLINT) --noout --dtdvalid $(dtddir)/wayland.dtd $^ > $@

validate-protocol-xml : $(protocol_sources:protocol/%.xml=.%.xml.valid)

CLEANFILES += $(protocol_sources:protocol/%.xml=.%.xml.valid)
EXTRA_DIST += $(protocol_sources)

endif


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


wayland_protocoldir = $(top_srcdir)/protocol
include $(top_srcdir)/wayland-scanner.mk
