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
	src/google/airbag_types.h \
	src/google/minidump_processor.h \
	src/google/stack_frame.h \
	src/google/symbol_supplier.h \
	src/processor/contained_range_map.h \
	src/processor/contained_range_map-inl.h \
	src/processor/linked_ptr.h \
	src/processor/memory_region.h \
	src/processor/minidump.cc \
	src/processor/minidump.h \
	src/processor/minidump_format.h \
	src/processor/minidump_processor.cc \
	src/processor/range_map.h \
	src/processor/range_map-inl.h \
	src/processor/source_line_resolver.cc \
	src/processor/source_line_resolver.h \
	src/processor/stackwalker.cc \
	src/processor/stackwalker.h \
	src/processor/stackwalker_x86.cc \
	src/processor/stackwalker_x86.h


## Programs
bin_PROGRAMS = \
	src/processor/minidump_dump \
	src/processor/minidump_stackwalk


## Tests
check_PROGRAMS = \
	src/processor/contained_range_map_unittest \
	src/processor/minidump_processor_unittest \
	src/processor/range_map_unittest \
	src/processor/source_line_resolver_unittest
check_SCRIPTS = \
	src/processor/minidump_dump_test \
	src/processor/minidump_stackwalk_test
TESTS = $(check_PROGRAMS) $(check_SCRIPTS)
TESTS_ENVIRONMENT =

src_processor_contained_range_map_unittest_SOURCES = \
	src/processor/contained_range_map_unittest.cc

src_processor_minidump_processor_unittest_SOURCES = \
	src/processor/minidump_processor_unittest.cc
src_processor_minidump_processor_unittest_LDADD = \
	src/processor/minidump_processor.lo \
	src/processor/minidump.lo \
	src/processor/stackwalker.lo \
	src/processor/stackwalker_x86.lo \
	src/processor/source_line_resolver.lo

src_processor_range_map_unittest_SOURCES = \
	src/processor/range_map_unittest.cc

src_processor_source_line_resolver_unittest_SOURCES = \
	src/processor/source_line_resolver_unittest.cc
src_processor_source_line_resolver_unittest_LDADD = \
	src/processor/source_line_resolver.lo

## Non-installables
noinst_PROGRAMS =
noinst_SCRIPTS = $(check_SCRIPTS)

src_processor_minidump_dump_SOURCES = \
	src/processor/minidump_dump.cc
src_processor_minidump_dump_LDADD = \
	src/processor/minidump.lo

src_processor_minidump_stackwalk_SOURCES = \
	src/processor/minidump_stackwalk.cc
src_processor_minidump_stackwalk_LDADD = \
	src/processor/minidump.lo \
	src/processor/stackwalker.lo \
	src/processor/stackwalker_x86.lo \
	src/processor/source_line_resolver.lo


## Additional files to be included in a source distribution
EXTRA_DIST = \
	$(SCRIPTS) \
	src/processor/testdata/minidump1.dmp \
	src/processor/testdata/minidump1.out \
	src/processor/testdata/minidump1.stack.out \
	src/processor/testdata/minidump2.dmp \
	src/processor/testdata/minidump2.sym \
	src/processor/testdata/module1.out \
	src/processor/testdata/module2.out \
	src/processor/testdata/module3_bad.out


## Additional rules
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
