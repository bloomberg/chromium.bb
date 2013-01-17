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
VALID_TOOLCHAINS:=newlib glibc pnacl linux win


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
TARGET=ppapi_cpp

#
# List of sources to compile
#
SOURCES:=ppp_entrypoints.cc
SOURCES+=array_output.cc audio.cc audio_config.cc core.cc
SOURCES+=file_io.cc file_ref.cc file_system.cc fullscreen.cc
SOURCES+=graphics_2d.cc graphics_3d.cc graphics_3d_client.cc
SOURCES+=image_data.cc input_event.cc instance.cc instance_handle.cc
SOURCES+=lock.cc message_loop.cc module.cc mouse_cursor.cc mouse_lock.cc
SOURCES+=rect.cc resource.cc
SOURCES+=url_loader.cc url_request_info.cc url_response_info.cc
SOURCES+=var.cc var_array_buffer.cc view.cc websocket.cc

# Utility sources.
SOURCES+=paint_aggregator.cc paint_manager.cc simple_thread.cc
SOURCES+=websocket_api.cc

#
# Use the compile macro for each source.
#
$(foreach src,$(SOURCES),$(eval $(call COMPILE_RULE,$(src),-Wno-switch-enum)))

#
# Use the lib macro for this target on the list of sources.
#
$(eval $(call LIB_RULE,$(TARGET),$(SOURCES)))
