
noinst_DATA = xml/client/index.xml xml/server/index.xml
dist_noinst_DATA = wayland.doxygen.in

scanned_src_files_shared = 				\
	$(top_srcdir)/src/wayland-util.c	\
	$(top_srcdir)/src/wayland-util.h

scanned_src_files_client = 				\
	$(scanned_src_files_shared)			\
	$(top_srcdir)/src/wayland-client.c	\
	$(top_srcdir)/src/wayland-client.h

scanned_src_files_server = 				\
	$(scanned_src_files_shared)			\
	$(top_srcdir)/src/wayland-server.c	\
	$(top_srcdir)/src/wayland-server.h	\
	$(top_srcdir)/src/wayland-shm.c

# find all man/man3/wl_foo.3 pages
# for this to work, we need to create them before the man target (hence
# all-local below)
dist_man3_MANS = $(shell test -d man && find man/man3 -name "wl_*.3" -printf "man/man3/%P\n")

xml/client/index.xml: $(scanned_src_files_client) wayland.doxygen
	$(AM_V_GEN)$(MKDIR_P) xml/client && \
	(cat wayland.doxygen; \
          echo "GENERATE_XML=YES"; \
          echo "XML_OUTPUT=xml/client"; \
          echo "INPUT= $(scanned_src_files_client)"; \
          ) | doxygen -
xml/server/index.xml: $(scanned_src_files_server) wayland.doxygen
	$(AM_V_GEN)$(MKDIR_P) xml/server && \
	(cat wayland.doxygen; \
          echo "GENERATE_XML=YES"; \
          echo "XML_OUTPUT=xml/server"; \
          echo "INPUT= $(scanned_src_files_server)"; \
          ) | doxygen -

man/man3/wl_display.3: $(scanned_src_files_client) $(scanned_src_files_server)
	$(AM_V_GEN)(cat wayland.doxygen; \
          echo "GENERATE_MAN=YES"; \
          echo "MAN_OUTPUT=man"; \
          echo "JAVADOC_AUTOBRIEF=NO"; \
          echo "INPUT= $^"; \
          ) | doxygen -

# there is no man-local
all-local: man/man3/wl_display.3

clean-local:
	rm -rf xml/
	rm -rf man/
