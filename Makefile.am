if BUILD_DOCS
doc_subdir = doc
endif

SUBDIRS = $(doc_subdir) . tests

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

aclocaldir = $(datadir)/aclocal
dist_aclocal_DATA = wayland-scanner.m4

dist_pkgdata_DATA =				\
	wayland-scanner.mk			\
	protocol/wayland.xml			\
	protocol/wayland.dtd

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA =

lib_LTLIBRARIES = libwayland-server.la libwayland-client.la
noinst_LTLIBRARIES = libwayland-util.la

include_HEADERS =				\
	src/wayland-util.h			\
	src/wayland-server-protocol.h		\
	src/wayland-server.h			\
	src/wayland-client-protocol.h		\
	src/wayland-client.h			\
	src/wayland-egl.h			\
	src/wayland-version.h

libwayland_util_la_SOURCES =			\
	src/connection.c			\
	src/wayland-util.c			\
	src/wayland-util.h			\
	src/wayland-os.c			\
	src/wayland-os.h			\
	src/wayland-private.h

libwayland_server_la_CFLAGS = $(FFI_CFLAGS) $(GCC_CFLAGS) -pthread
libwayland_server_la_LIBADD = $(FFI_LIBS) libwayland-util.la -lrt -lm
libwayland_server_la_LDFLAGS = -version-info 1:0:1
libwayland_server_la_SOURCES =			\
	src/wayland-server.c			\
	src/wayland-shm.c			\
	src/event-loop.c

nodist_libwayland_server_la_SOURCES =		\
	protocol/wayland-protocol.c

libwayland_client_la_CFLAGS = $(FFI_CFLAGS) $(GCC_CFLAGS) -pthread
libwayland_client_la_LIBADD = $(FFI_LIBS) libwayland-util.la -lrt -lm
libwayland_client_la_LDFLAGS = -version-info 2:0:2
libwayland_client_la_SOURCES =			\
	src/wayland-client.c

nodist_libwayland_client_la_SOURCES =		\
	protocol/wayland-protocol.c

pkgconfig_DATA += src/wayland-client.pc src/wayland-server.pc

protocoldir = $(top_srcdir)/protocol

if ENABLE_SCANNER
wayland_scanner = $(top_builddir)/wayland-scanner
else
wayland_scanner = wayland-scanner
endif

protocol/%-protocol.c : $(top_srcdir)/protocol/%.xml
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(wayland_scanner) code < $< > $@

protocol/%-server-protocol.h : $(top_srcdir)/protocol/%.xml
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(wayland_scanner) server-header < $< > $@

protocol/%-client-protocol.h : $(top_srcdir)/protocol/%.xml
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(wayland_scanner) client-header < $< > $@


if ENABLE_SCANNER
bin_PROGRAMS = wayland-scanner

wayland_scanner_SOURCES = src/scanner.c
wayland_scanner_LDADD = $(EXPAT_LIBS) libwayland-util.la

$(BUILT_SOURCES) : wayland-scanner

pkgconfig_DATA += src/wayland-scanner.pc
endif

BUILT_SOURCES =					\
	$(nodist_libwayland_server_la_SOURCES)	\
	$(nodist_libwayland_client_la_SOURCES)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = src/wayland-version.h
EXTRA_DIST = src/wayland-version.h.in



lib_LTLIBRARIES += libwayland-cursor.la

include_HEADERS += cursor/wayland-cursor.h

libwayland_cursor_la_SOURCES =			\
	cursor/wayland-cursor.c			\
	cursor/os-compatibility.c		\
	cursor/os-compatibility.h		\
	cursor/cursor-data.h			\
	cursor/xcursor.c			\
	cursor/xcursor.h
libwayland_cursor_la_LIBADD = libwayland-client.la

pkgconfig_DATA += cursor/wayland-cursor.pc

libwayland_cursor_la_CFLAGS =			\
	$(GCC_CFLAGS)				\
	-I$(top_builddir)/src			\
	-I$(top_srcdir)/src			\
	-DICONDIR=\"$(ICONDIR)\"
