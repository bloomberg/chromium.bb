if BUILD_DOCS
doc_subdir = doc
endif

SUBDIRS = src protocol $(doc_subdir) tests cursor

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

aclocaldir = $(datadir)/aclocal
aclocal_DATA = wayland-scanner.m4 wayland-scanner.mk

EXTRA_DIST = wayland-scanner.mk
