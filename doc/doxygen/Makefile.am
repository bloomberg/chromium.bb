
noinst_DATA = xml/index.xml
dist_noinst_DATA = wayland.doxygen.in

scanned_src_files = 				\
	$(top_srcdir)/src/wayland-client.c	\
	$(top_srcdir)/src/wayland-client.h

xml/index.xml: $(scanned_src_files)
	doxygen wayland.doxygen

clean-local:
	rm -rf xml/
