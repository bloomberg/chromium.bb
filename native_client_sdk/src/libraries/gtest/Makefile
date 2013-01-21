# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
#   http://www.gnu.org/software/make/manual/make.html
#
#

# Default configuration
#
# By default we will build a Debug configuration using the GCC newlib toolcahin
# to override this, specify TOOLCHAIN=newlib|glibc or CONFIG=Debug|Release on
# the make command-line or in this file prior to including common.mk.  The
# toolchain we use by default will be the first valid one listed
VALID_TOOLCHAINS:=newlib glibc linux win


#
# Get pepper directory for toolchain and includes.
#
# If NACL_SDK_ROOT is not set, then assume it can be found relative to
# to this Makefile.
#
NACL_SDK_ROOT?=$(abspath $(CURDIR)/../..)
EXTRA_INC_PATHS=$(NACL_SDK_ROOT)/include/gtest/internal

include $(NACL_SDK_ROOT)/tools/common.mk


#
# Target Name
#
# The base name of the final library, also the name of the NMF file containing
# the mapping between architecture and actual NEXE.
#
TARGET=gtest

#
# List of sources to compile
#
SOURCES:=gtest.cc gtest-death-test.cc gtest-filepath.cc gtest_main.cc
SOURCES+=gtest-port.cc gtest-printers.cc gtest-test-part.cc
SOURCES+=gtest-typed-test.cc nacl_gtest_dummy_sys.cc



#
# Use the compile macro for each source.
#
$(foreach src,$(SOURCES),$(eval $(call COMPILE_RULE,$(src),-Wno-switch-enum -Wno-variadic-macros)))

#
# Use the lib macro for this target on the list of sources.
#
$(eval $(call LIB_RULE,$(TARGET),$(SOURCES)))
