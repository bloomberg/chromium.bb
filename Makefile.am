bin_PROGRAMS =
moduledir = $(libdir)/weston
module_LTLIBRARIES =
BUILT_SOURCES =

SUBDIRS =					\
	shared					\
	src					\
	clients					\
	protocol				\
	tests					\
	man

DISTCHECK_CONFIGURE_FLAGS = --disable-setuid-install

EXTRA_DIST = weston.ini.in wayland-scanner.mk

weston.ini : $(srcdir)/weston.ini.in
	$(AM_V_GEN)$(SED) \
		-e 's|@bindir[@]|$(bindir)|g' \
		-e 's|@abs_top_builddir[@]|$(abs_top_builddir)|g' \
		-e 's|@libexecdir[@]|$(libexecdir)|g' \
		$< > $@

all-local : weston.ini

CLEANFILES = weston.ini $(BUILT_SOURCES)


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

wayland_protocoldir = $(top_srcdir)/protocol
include $(top_srcdir)/wayland-scanner.mk
