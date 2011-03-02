noinst_PROGRAMS = compositor

AM_CPPFLAGS =					\
	-DDATADIR='"$(datadir)"'		\
	-I$(top_builddir)/wayland		\
	-I$(top_srcdir)/wayland			\
	$(COMPOSITOR_CFLAGS)

AM_CFLAGS = $(GCC_CFLAGS)

compositor_LDADD =				\
	$(COMPOSITOR_LIBS)

if ENABLE_DRM_COMPOSITOR
drm_compositor_sources = compositor-drm.c tty.c evdev.c
endif

if ENABLE_X11_COMPOSITOR
x11_compositor_sources = compositor-x11.c
endif

if ENABLE_WAYLAND_COMPOSITOR
wayland_compositor_sources = compositor-wayland.c
endif

compositor_SOURCES =				\
	compositor.c				\
	compositor.h				\
	shell.c					\
	screenshooter.c				\
	screenshooter-protocol.c		\
	screenshooter-server-protocol.h		\
	shm.c					\
	$(drm_compositor_sources)		\
	$(x11_compositor_sources)		\
	$(wayland_compositor_sources)

udevrulesddir = $(sysconfdir)/udev/rules.d

dist_udevrulesd_DATA =				\
	70-wayland.rules

BUILT_SOURCES =					\
	screenshooter-server-protocol.h		\
	screenshooter-protocol.c

CLEANFILES = $(BUILT_SOURCES)

@wayland_scanner_rules@
