if BUILD_DOCS
doc_subdir = doc
endif

SUBDIRS = src protocol $(doc_subdir) tests cursor

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

aclocaldir = $(datadir)/aclocal
dist_aclocal_DATA = wayland-scanner.m4

dist_pkgdata_DATA = wayland-scanner.mk
