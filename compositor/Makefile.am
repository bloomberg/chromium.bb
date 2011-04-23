noinst_PROGRAMS = compositor

AM_CPPFLAGS =					\
	-DDATADIR='"$(datadir)"'		\
	$(COMPOSITOR_CFLAGS)			\
	$(X11_COMPOSITOR_CFLAGS)		\
	$(DRM_COMPOSITOR_CFLAGS)		\
	$(WAYLAND_COMPOSITOR_CFLAGS)		\
	$(OPENWFD_COMPOSITOR_CFLAGS)

AM_CFLAGS = $(GCC_CFLAGS)

compositor_LDFLAGS = -export-dynamic
compositor_LDADD =				\
	$(COMPOSITOR_LIBS)			\
	$(X11_COMPOSITOR_LIBS)			\
	$(DRM_COMPOSITOR_LIBS)			\
	$(WAYLAND_COMPOSITOR_LIBS)		\
	$(OPENWFD_COMPOSITOR_LIBS)		\
	$(DLOPEN_LIBS)

if ENABLE_DRM_COMPOSITOR
drm_compositor_sources = compositor-drm.c tty.c evdev.c
endif

if ENABLE_X11_COMPOSITOR
x11_compositor_sources = compositor-x11.c
endif

if ENABLE_WAYLAND_COMPOSITOR
wayland_compositor_sources = compositor-wayland.c
endif

if ENABLE_OPENWFD_COMPOSITOR
openwfd_compositor_sources = compositor-openwfd.c
endif

compositor_SOURCES =				\
	compositor.c				\
	compositor.h				\
	image-loader.c				\
	shell.c					\
	switcher.c				\
	screenshooter.c				\
	screenshooter-protocol.c		\
	screenshooter-server-protocol.h		\
	$(drm_compositor_sources)		\
	$(openwfd_compositor_sources)		\
	$(x11_compositor_sources)		\
	$(wayland_compositor_sources)

udevrulesddir = $(sysconfdir)/udev/rules.d

dist_udevrulesd_DATA =				\
	70-wayland.rules

meego_tablet_shell_la_LTLIBRARIES = meego-tablet-shell.la
meego_tablet_shell_la_LDFLAGS = -module -avoid-version
meego_tablet_shell_ladir = @libdir@/wayland
meego_tablet_shell_la_LIBADD = $(COMPOSITOR_LIBS)

meego_tablet_shell_la_SOURCES =			\
	meego-tablet-shell.c			\
	meego-tablet-shell.h			\
	meego-tablet-protocol.c			\
	meego-tablet-server-protocol.h

BUILT_SOURCES =					\
	screenshooter-server-protocol.h		\
	screenshooter-protocol.c		\
	meego-tablet-protocol.c			\
	meego-tablet-server-protocol.h

CLEANFILES = $(BUILT_SOURCES)

@wayland_scanner_rules@
