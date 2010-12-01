noinst_PROGRAMS = compositor

INCLUDES =					\
	-I$(top_srcdir)/wayland			\
	-I$(top_builddir)/wayland		\
	$(COMPOSITOR_CFLAGS)

AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS = -DDATADIR='"$(datadir)"'

compositor_LDADD =					\
	$(top_builddir)/wayland/libwayland-server.la	\
	$(top_builddir)/wayland/libwayland-client.la	\
	$(COMPOSITOR_LIBS)

compositor_SOURCES =				\
	compositor.c				\
	compositor.h				\
	compositor-drm.c			\
	compositor-x11.c			\
	compositor-wayland.c			\
	screenshooter.c				\
	screenshooter-protocol.c		\
	screenshooter-server-protocol.h		\
	drm.c					\
	shm.c

udevrulesddir = $(sysconfdir)/udev/rules.d

dist_udevrulesd_DATA =				\
	70-wayland.rules

BUILT_SOURCES =					\
	screenshooter-server-protocol.h		\
	screenshooter-protocol.c

CLEANFILES = $(BUILT_SOURCES)

include $(top_srcdir)/wayland/scanner.mk
