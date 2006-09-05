## Process this file with automake to produce Makefile.in

# Copyright (C) 2006 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


# This allows #includes to be relative to src/
AM_CPPFLAGS = -I$(top_srcdir)/src

## Documentation
docdir = $(prefix)/share/doc/$(PACKAGE)-$(VERSION)

dist_doc_DATA = \
	AUTHORS \
	COPYING \
	ChangeLog \
	INSTALL \
	NEWS \
	README


## Libraries
lib_LTLIBRARIES = src/libairbag.la

src_libairbag_la_SOURCES = \
	src/processor/source_line_resolver.cc \
	src/processor/source_line_resolver.h


## Tests
TESTS = src/processor/source_line_resolver_unittest
TESTS_ENVIRONMENT =
check_SCRIPTS =

src_processor_source_line_resolver_unittest_SOURCES = \
	src/processor/source_line_resolver_unittest.cc
src_processor_source_line_resolver_unittest_LDADD = \
	src/processor/source_line_resolver.lo


## Non-installables
noinst_PROGRAMS = $(TESTS)
noinst_SCRIPTS = $(check_SCRIPTS)


## Additional files to be included in a source distribution
EXTRA_DIST = \
	$(SCRIPTS) \
	src/processor/testdata/module1.out \
	src/processor/testdata/module2.out \
	src/processor/testdata/module3_bad.out


## Additional rules
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
