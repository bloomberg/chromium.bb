## Process this file with automake to produce Makefile.in

# Make sure that when we re-make ./configure, we get the macros we need
ACLOCAL_AMFLAGS = -I `pwd`/../autoconf

# This is so we can #include <google/foo>
AM_CPPFLAGS = -I$(top_srcdir)/src

googleincludedir = $(includedir)/google
## The .h files you want to install (that is, .h files that people
## who install this package can include in their own applications.)
googleinclude_HEADERS =

docdir = $(prefix)/share/doc/$(PACKAGE)-$(VERSION)
## This is for HTML and other documentation you want to install.
## Add your documentation files (in doc/) in addition to these
## top-level boilerplate files.  Also add a TODO file if you have one.
dist_doc_DATA = AUTHORS COPYING ChangeLog INSTALL NEWS README

## The libraries (.so's) you want to install
lib_LTLIBRARIES = libairbag.la

## unittests you want to run when people type 'make check'.
## TESTS is for binary unittests, check_SCRIPTS for script-based unittests.
## TESTS_ENVIRONMENT sets environment variables for when you run unittest,
## but it only seems to take effect for *binary* unittests (argh!)
TESTS = source_line_resolver_unittest
TESTS_ENVIRONMENT =
check_SCRIPTS =
# Every time you add a unittest to check_SCRIPTS, add it here too
noinst_SCRIPTS =

## vvvv RULES TO MAKE THE LIBRARIES, BINARIES, AND UNITTESTS

libairbag_la_SOURCES = 				\
	src/processor/source_line_resolver.cc	\
	src/processor/source_line_resolver.h

source_line_resolver_unittest_SOURCES =		\
	src/processor/source_line_resolver_unittest.cc \
	src/processor/source_line_resolver.h
source_line_resolver_unittest_LDADD = source_line_resolver.lo

## ^^^^ END OF RULES TO MAKE THE LIBRARIES, BINARIES, AND UNITTESTS


## This should always include $(TESTS), but may also include other
## binaries that you compile but don't want automatically installed.
noinst_PROGRAMS = $(TESTS)

rpm: dist-gzip packages/rpm.sh packages/rpm/rpm.spec
	@cd packages && ./rpm.sh ${PACKAGE} ${VERSION}

deb: dist-gzip packages/deb.sh packages/deb/*
	@cd packages && ./deb.sh ${PACKAGE} ${VERSION}

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
EXTRA_DIST = packages/rpm.sh packages/rpm/rpm.spec packages/deb.sh packages/deb \
	$(SCRIPTS) libtool

## If you create hash_map.h, hash_set.h, and/or hash_fun.h via the
## ACC_CXX_MAKE_*_H configure.ac macros, add those files here.
DISTCLEANFILES =
