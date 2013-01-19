# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
#   http://www.gnu.org/software/make/manual/make.html
#
#


#
# Default configuration
#
# By default we will build a Debug configuration using the GCC newlib toolcahin
# to override this, specify TOOLCHAIN=newlib|glibc or CONFIG=Debug|Release on
# the make command-line or in this file prior to including common.mk.  The
# toolchain we use by default will be the first valid one listed
VALID_TOOLCHAINS:=newlib glibc win


#
# Get pepper directory for toolchain and includes.
#
# If NACL_SDK_ROOT is not set, then assume it can be found relative to
# to this Makefile.
#
NACL_SDK_ROOT?=$(abspath $(CURDIR)/../..)
include $(NACL_SDK_ROOT)/tools/common.mk


#
# Target Name
#
# The base name of the final library, also the name of the NMF file containing
# the mapping between architecture and actual NEXE.
#
TARGET=nacl_mounts

#
# List of sources to compile
#
SOURCES:=kernel_handle.cc kernel_intercept.cc kernel_object.cc kernel_proxy.cc
SOURCES+=kernel_wrap_glibc.cc kernel_wrap_newlib.cc kernel_wrap_win.cc
SOURCES+=mount.cc mount_dev.cc mount_html5fs.cc mount_http.cc mount_mem.cc
SOURCES+=mount_node.cc mount_node_dir.cc mount_node_html5fs.cc mount_node_mem.cc
SOURCES+=nacl_mounts.cc path.cc pepper_interface.cc real_pepper_interface.cc

#
# Use the compile macro for each source.
#
$(foreach src,$(SOURCES),$(eval $(call COMPILE_RULE,$(src))))

#
# Use the lib macro for this target on the list of sources.
#
$(eval $(call LIB_RULE,$(TARGET),$(SOURCES)))
