moduledir = $(libdir)/weston
module_LTLIBRARIES = $(desktop_shell)

AM_CPPFLAGS =					\
	-I$(top_srcdir)/shared			\
	-I$(top_srcdir)/src			\
	-I$(top_builddir)/src			\
	-DDATADIR='"$(datadir)"'		\
	-DMODULEDIR='"$(moduledir)"'		\
	-DLIBEXECDIR='"$(libexecdir)"'		\
	-DIN_WESTON

if ENABLE_DESKTOP_SHELL
desktop_shell = desktop-shell.la
desktop_shell_la_LDFLAGS = -module -avoid-version
desktop_shell_la_LIBADD = $(COMPOSITOR_LIBS)	\
	../shared/libshared.la
desktop_shell_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)
desktop_shell_la_SOURCES =			\
	shell.h					\
	shell.c					\
	exposay.c				\
	input-panel.c				\
	desktop-shell-protocol.c		\
	desktop-shell-server-protocol.h 	\
	xdg-shell-protocol.c			\
	xdg-shell-server-protocol.h
endif

BUILT_SOURCES =					\
	desktop-shell-protocol.c		\
	desktop-shell-server-protocol.h		\
	xdg-shell-protocol.c			\
	xdg-shell-server-protocol.h

CLEANFILES = $(BUILT_SOURCES)

wayland_protocoldir = $(top_srcdir)/protocol
include $(top_srcdir)/wayland-scanner.mk
