# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
#   http://www.gnu.org/software/make/manual/make.html
#

#
# Project information
#
# These variables store project specific settings for the project name
# build flags, files to copy or install.  In the examples it is typically
# only the list of sources and project name that will actually change and
# the rest of the makefile is boilerplate for defining build rules.
#
PROJECT:=fullscreen_tumbler
C_SOURCES:=
CXX_SOURCES:=transforms.cc shader_util.cc opengl_context.cc tumbler_module.cc
CXX_SOURCES+=scripting_bridge.cc tumbler.cc cube.cc
COPY_FILES:=bind.js trackball.js dragger.js vector3.js tumbler.js
COPY_FILES+=fullscreen_tumbler.html fullscreen_tumbler.nmf
COPY_FILES+=../common/check_browser.js


#
# Get pepper directory for toolchain and includes.
#
# If PEPPER_ROOT is not set, then assume it can be found a two directories up,
# from the default example directory location.
#
THIS_MAKEFILE:=$(abspath $(lastword $(MAKEFILE_LIST)))
PEPPER_ROOT?=$(abspath $(dir $(THIS_MAKEFILE))../..)

# Project Build flags
DEFINES:=
INCLUDES:=
WARNINGS:=-Wno-long-long -Wall -Wswitch-enum -pedantic -Werror
CXXFLAGS:=-pthread -std=gnu++98 $(WARNINGS) $(DEFINES) $(INCLUDES)
LDFLAGS:=-lppapi_gles2 -lppapi_cpp -lppapi

#
# Compute tool paths
#
#
OSNAME:=$(shell python $(PEPPER_ROOT)/tools/getos.py)
TC_PATH:=$(abspath $(PEPPER_ROOT)/toolchain/$(OSNAME)_x86_newlib)
CC:=$(TC_PATH)/bin/i686-nacl-gcc
CXX:=$(TC_PATH)/bin/i686-nacl-g++
STRIP:=$(TC_PATH)/bin/i686-nacl-strip

#
# Create shell aliases
#
# Create Python based aliases for common shell commands like copy or move.
#
COPY = python $(PEPPER_ROOT)/tools/oshelpers.py cp
MKDIR = python $(PEPPER_ROOT)/tools/oshelpers.py mkdir
RM = python $(PEPPER_ROOT)/tools/oshelpers.py rm
MV = python $(PEPPER_ROOT)/tools/oshelpers.py mv

#
# Disable DOS PATH warning when using Cygwin based tools Windows
#
CYGWIN ?= nodosfilewarning
export CYGWIN

#
# Define a macro for copying files to the configuration directory
# 
# Copys a source file to the destination directory, removing the base path
# from the source.  Adds a dependency to the destination directory in case it
# needs to be created.
#
# $(1) = Source file
# $(2) = Destination directory
define FILE_COPY
$(2)/$(notdir $(1)) : $(1) | $(2)
	$(COPY) $(1) $(2)
$(2)_COPIES+=$(2)/$(notdir $(1))
endef


# Declare the ALL target first, to make the 'all' target the default build
all: DEBUG RELEASE


#
# Debug Build rules.
#
DEBUG_x86_32_FLAGS:=-m32 -O0 -g
DEBUG_x86_64_FLAGS:=-m64 -O0 -g
DEBUG_x86_32_OBJS:=$(patsubst %.cc,DBG/x86_32/%.o,$(CXX_SOURCES))
DEBUG_x86_64_OBJS:=$(patsubst %.cc,DBG/x86_64/%.o,$(CXX_SOURCES))

# Create DBG configuration directories
DBG:
	$(MKDIR) -p $@

DBG/x86_32:
	$(MKDIR) -p $@

DBG/x86_64:
	$(MKDIR) -p $@

# Copy all files to that config
$(foreach src,$(COPY_FILES),$(eval $(call FILE_COPY,$(src),DBG)))

# Include generated dependencies
-include DBG/x86_32/*.d
-include DBG/x86_64/*.d

# Define compile rule for all 32 bit debug objects
DBG/x86_32/%.o : %.cc $(THIS_MAKE) | DBG/x86_32
	$(CXX) -o $@ -c $< $(DEBUG_x86_32_FLAGS) $(CXXFLAGS) -MMD -MF $@.d

# Define compile rule for all 64 bit debug objects
DBG/x86_64/%.o : %.cc $(THIS_MAKE) | DBG/x86_64
	$(CXX) -o $@ -c $< $(DEBUG_x86_64_FLAGS) $(CXXFLAGS)

# Define Link rule for 32 bit debug NEXE
DBG/$(PROJECT)_x86_32.nexe : $(DEBUG_x86_32_OBJS)
	$(CXX) -o $@ $^ $(DEBUG_x86_32_FLAGS) $(LDFLAGS)

# Define Link rule for 64 bit debug NEXE
DBG/$(PROJECT)_x86_64.nexe : $(DEBUG_x86_64_OBJS)
	$(CXX) -o $@ $^ $(DEBUG_x86_64_FLAGS) $(LDFLAGS)

# Define a DEBUG alias to build the debug version
.PHONY : DEBUG RUN_DEBUG
DEBUG : DBG/$(PROJECT)_x86_32.nexe DBG/$(PROJECT)_x86_64.nexe $(DBG_COPIES)

# Define a RUN_DEBUG alias to build and server the DEBUG version
RUN_DEBUG: DEBUG
	cd DBG && python ../../httpd.py


#
# Release build rules.
#
RELEASE_x86_32_FLAGS:=-m32 -O2 -g
RELEASE_x86_64_FLAGS:=-m64 -O2 -g
RELEASE_x86_32_OBJS:=$(patsubst %.cc,REL/x86_32/%.o,$(CXX_SOURCES))
RELEASE_x86_64_OBJS:=$(patsubst %.cc,REL/x86_64/%.o,$(CXX_SOURCES))

REL:
	$(MKDIR) -p $@

REL/x86_32:
	$(MKDIR) -p $@

REL/x86_64:
	$(MKDIR) -p $@

# Copy all files to that config
$(foreach src,$(COPY_FILES),$(eval $(call FILE_COPY,$(src),REL)))

# Include generated dependencies
-include REL/x86_32/*.d
-include REL/x86_64/*.d

# Define compile rule for all 32 bit debug objects
REL/x86_32/%.o : %.cc $(THIS_MAKE) | REL/x86_32
	$(CXX) -o $@ -c $< $(RELEASE_x86_32_FLAGS) $(CXXFLAGS) -MMD -MF $@.d

# Define compile rule for all 64 bit debug objects
REL/x86_64/%.o : %.cc $(THIS_MAKE) | REL/x86_64
	$(CXX) -o $@ -c $< $(RELEASE_x86_64_FLAGS) $(CXXFLAGS)

# Define Link rule for 32 bit optimized and stripped NEXE
REL/$(PROJECT)_x86_32.nexe : $(RELEASE_x86_32_OBJS)
	$(CXX) -o $@.unstripped $^ $(RELEASE_x86_32_FLAGS) $(LDFLAGS)
	$(STRIP) $< -o $@

# Define Link rule for 64 bit optimized and stripped NEXE
REL/$(PROJECT)_x86_64.nexe : $(RELEASE_x86_64_OBJS)
	$(CXX) -o $@.unstripped $^ $(RELEASE_x86_64_FLAGS) $(LDFLAGS)
	$(STRIP) $@.unstripped -o $@

# Define a RELEASE alias to build the debug version
.PHONY : RELEASE RUN_RELEASE
RELEASE : REL/$(PROJECT)_x86_32.nexe REL/$(PROJECT)_x86_64.nexe $(REL_COPIES)

# Define a RUN_RELEASE alias to build and server the RELEASE version
RUN_RELEASE: RELEASE
	cd REL && python ../../httpd.py

