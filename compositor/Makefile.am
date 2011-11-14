bin_PROGRAMS = wayland-compositor

AM_CPPFLAGS =					\
	-DDATADIR='"$(datadir)"'		\
	-DMODULEDIR='"$(moduledir)"'		\
	-DLIBEXECDIR='"$(libexecdir)"'		\
	-DXSERVER_PATH='"@XSERVER_PATH@"'	\
	$(COMPOSITOR_CFLAGS)

wayland_compositor_LDFLAGS = -export-dynamic
wayland_compositor_CFLAGS = $(GCC_CFLAGS)
wayland_compositor_LDADD = \
	$(COMPOSITOR_LIBS) $(DLOPEN_LIBS) $(XSERVER_LAUNCHER_LIBS)

wayland_compositor_SOURCES =			\
	compositor.c				\
	compositor.h				\
	image-loader.c				\
	screenshooter.c				\
	screenshooter-protocol.c		\
	screenshooter-server-protocol.h		\
	$(xserver_launcher_sources)

if ENABLE_XSERVER_LAUNCHER
xserver_launcher_sources =			\
	xserver-launcher.c			\
	xserver-protocol.c			\
	xserver-server-protocol.h
endif

moduledir = @libdir@/wayland
module_LTLIBRARIES =				\
	$(desktop_shell)			\
	$(tablet_shell)			\
	$(x11_backend)				\
	$(drm_backend)				\
	$(wayland_backend)			\
	$(openwfd_backend)

if ENABLE_X11_COMPOSITOR
x11_backend = x11-backend.la
x11_backend_la_LDFLAGS = -module -avoid-version
x11_backend_la_LIBADD = $(COMPOSITOR_LIBS) $(X11_COMPOSITOR_LIBS)
x11_backend_la_CFLAGS = $(X11_COMPOSITOR_CFLAGS) $(GCC_CFLAGS)
x11_backend_la_SOURCES = compositor-x11.c
endif

if ENABLE_DRM_COMPOSITOR
drm_backend = drm-backend.la
drm_backend_la_LDFLAGS = -module -avoid-version
drm_backend_la_LIBADD = $(COMPOSITOR_LIBS) $(DRM_COMPOSITOR_LIBS)
drm_backend_la_CFLAGS = $(DRM_COMPOSITOR_CFLAGS) $(GCC_CFLAGS)
drm_backend_la_SOURCES = compositor-drm.c tty.c evdev.c
endif

if ENABLE_WAYLAND_COMPOSITOR
wayland_backend = wayland-backend.la
wayland_backend_la_LDFLAGS = -module -avoid-version
wayland_backend_la_LIBADD = $(COMPOSITOR_LIBS) $(WAYLAND_COMPOSITOR_LIBS)
wayland_backend_la_CFLAGS = $(WAYLAND_COMPOSITOR_CFLAGS) $(GCC_CFLAGS)
wayland_backend_la_SOURCES = compositor-wayland.c
endif

if ENABLE_OPENWFD_COMPOSITOR
openwfd_backend = openwfd-backend.la
openwfd_backend_la_LDFLAGS = -module -avoid-version
openwfd_backend_la_LIBADD = $(COMPOSITOR_LIBS) $(OPENWFD_COMPOSITOR_LIBS)
openwfd_backend_la_CFLAGS = $(OPENWFD_COMPOSITOR_CFLAGS) $(GCC_CFLAGS)
openwfd_backend_la_SOURCES = compositor-openwfd.c tty.c evdev.c
endif

if ENABLE_DESKTOP_SHELL
desktop_shell = desktop-shell.la
desktop_shell_la_LDFLAGS = -module -avoid-version
desktop_shell_la_LIBADD = $(COMPOSITOR_LIBS)
desktop_shell_la_CFLAGS = $(GCC_CFLAGS)
desktop_shell_la_SOURCES =			\
	shell.c					\
	switcher.c				\
	desktop-shell-protocol.c		\
	desktop-shell-server-protocol.h
endif

if ENABLE_TABLET_SHELL
tablet_shell = tablet-shell.la
tablet_shell_la_LDFLAGS = -module -avoid-version
tablet_shell_la_LIBADD = $(COMPOSITOR_LIBS)
tablet_shell_la_CFLAGS = $(GCC_CFLAGS)
tablet_shell_la_SOURCES =			\
	tablet-shell.c				\
	tablet-shell.h				\
	tablet-shell-protocol.c			\
	tablet-shell-server-protocol.h
endif

BUILT_SOURCES =					\
	screenshooter-server-protocol.h		\
	screenshooter-protocol.c		\
	tablet-shell-protocol.c			\
	tablet-shell-server-protocol.h		\
	xserver-protocol.c			\
	xserver-server-protocol.h		\
	desktop-shell-protocol.c		\
	desktop-shell-server-protocol.h

CLEANFILES = $(BUILT_SOURCES)

@wayland_scanner_rules@
