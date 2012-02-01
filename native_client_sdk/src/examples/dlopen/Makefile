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
PROJECT:=dlopen
COPY_FILES:=dlopen.html
LDFLAGS:=-ldl -lppapi_cpp -lppapi

NEXES:=$(PROJECT)_x86_32.nexe $(PROJECT)_x86_64.nexe
NEXES+=lib32/libeightball.so lib64/libeightball.so

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
WARNINGS:=-Wno-long-long -Wall -Wswitch-enum
CXXFLAGS:=-pthread $(WARNINGS) $(DEFINES) $(INCLUDES)

#
# Compute tool paths
#
#
OSNAME:=$(shell python $(PEPPER_ROOT)/tools/getos.py)
TC_PATH:=$(abspath $(PEPPER_ROOT)/toolchain/$(OSNAME)_x86_glibc)
CC:=$(TC_PATH)/bin/i686-nacl-gcc
CXX:=$(TC_PATH)/bin/i686-nacl-g++
STRIP:=$(TC_PATH)/bin/i686-nacl-strip

#
# Create shell aliases
#
# Create Python based aliases for common shell commands like copy or move.
#
COPY:= python $(PEPPER_ROOT)/tools/oshelpers.py cp
MKDIR:= python $(PEPPER_ROOT)/tools/oshelpers.py mkdir
RM:= python $(PEPPER_ROOT)/tools/oshelpers.py rm
MV:= python $(PEPPER_ROOT)/tools/oshelpers.py mv

#
# NMF Manifiest generation
#
NMF:=python $(PEPPER_ROOT)/tools/create_nmf.py
NMF+=-D $(TC_PATH)/x86_64-nacl/bin/objdump
NMF_PATHS:=-L $(TC_PATH)/x86_64-nacl/lib32 -L $(TC_PATH)/x86_64-nacl/lib
NMF_PATHS+=-L lib32 -L lib64

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

# Create DBG configuration directories
DBG:
	$(MKDIR) -p $@

DBG/lib32:
	$(MKDIR) -p $@

DBG/lib64:
	$(MKDIR) -p $@

# Copy all files to that config
$(foreach src,$(COPY_FILES),$(eval $(call FILE_COPY,$(src),DBG)))

# Build debug version dlopen nexe and eightball.so for 32 and 64 bit.
DBG/dlopen_x86_32.o: dlopen.cc $(THIS_MAKE) | DBG 
	$(CXX) -o $@ -c $< $(DEBUG_x86_32_FLAGS) $(CXXFLAGS)

DBG/dlopen_x86_32.nexe: DBG/dlopen_x86_32.o $(THIS_MAKE) | DBG 
	$(CXX) -o $@ $< $(DEBUG_x86_32_FLAGS) $(LDFLAGS)

DBG/dlopen_x86_64.o: dlopen.cc $(THIS_MAKE) | DBG 
	$(CXX) -o $@ -c $< $(DEBUG_x86_64_FLAGS) $(CXXFLAGS)

DBG/dlopen_x86_64.nexe: DBG/dlopen_x86_64.o $(THIS_MAKE) | DBG 
	$(CXX) -o $@ $< $(DEBUG_x86_64_FLAGS) $(LDFLAGS)

DBG/eightball_x86_32.o: eightball.cc $(THIS_MAKE) | DBG
	$(CXX) -o $@ -c $< $(DEBUG_x86_32_FLAGS) $(CXXFLAGS) -fPIC

DBG/lib32/libeightball.so: DBG/eightball_x86_32.o $(THIS_MAKE) | DBG/lib32
	$(CXX) -o $@ $< $(DEBUG_x86_32_FLAGS) $(LDFLAGS) -shared

DBG/eightball_x86_64.o: eightball.cc $(THIS_MAKE) | DBG
	$(CXX) -o $@ -c $< $(DEBUG_x86_64_FLAGS) $(CXXFLAGS) -fPIC

DBG/lib64/libeightball.so: DBG/eightball_x86_64.o $(THIS_MAKE) | DBG/lib64
	$(CXX) -o $@ $< $(DEBUG_x86_64_FLAGS) $(LDFLAGS) -shared

# Define rule for building NMF file and copying dependencies
DBG_NEXES:=$(foreach src,$(NEXES),DBG/$(src))

DBG/$(PROJECT).nmf : $(DBG_NEXES)
	cd DBG && $(NMF) -o dlopen.nmf -s . $(NMF_PATHS) $(NEXES)

# Define a DEBUG alias to build the debug version
.PHONY : DEBUG RUN_DEBUG
DEBUG : $(DBG_NEXES) DBG/$(PROJECT).nmf $(DBG_COPIES)

# Define a RUN_DEBUG alias to build and server the DEBUG version
RUN_DEBUG: DEBUG
	cd DBG && python ../../httpd.py


#
# Release build rules.
#
RELEASE_x86_32_FLAGS:=-m32 -O2
RELEASE_x86_64_FLAGS:=-m64 -O2

REL:
	$(MKDIR) -p $@

REL/lib32:
	$(MKDIR) -p $@

REL/lib64:
	$(MKDIR) -p $@

# Copy all files to that config
$(foreach src,$(COPY_FILES),$(eval $(call FILE_COPY,$(src),REL)))

# Build release version dlopen nexe and eightball.so for 32 and 64 bit.
REL/dlopen_x86_32.o: dlopen.cc $(THIS_MAKE) | REL 
	$(CXX) -o $@ -c $< $(RELEASE_x86_32_FLAGS) $(CXXFLAGS)

REL/dlopen_x86_32.nexe: REL/dlopen_x86_32.o $(THIS_MAKE) | REL 
	$(CXX) -o $@ $< $(RELEASE_x86_32_FLAGS) $(LDFLAGS)

REL/dlopen_x86_64.o: dlopen.cc $(THIS_MAKE) | REL 
	$(CXX) -o $@ -c $< $(RELEASE_x86_64_FLAGS) $(CXXFLAGS)

REL/dlopen_x86_64.nexe: REL/dlopen_x86_64.o $(THIS_MAKE) | REL 
	$(CXX) -o $@ $< $(RELEASE_x86_64_FLAGS) $(LDFLAGS)

REL/eightball_x86_32.o: eightball.cc $(THIS_MAKE) | REL/lib32
	$(CXX) -o $@ -c $< $(RELEASE_x86_32_FLAGS) $(CXXFLAGS) -fPIC

REL/lib32/libeightball.so: REL/eightball_x86_32.o $(THIS_MAKE) | REL/lib32
	$(CXX) -o $@ $< $(RELEASE_x86_32_FLAGS) $(LDFLAGS) -shared

REL/eightball_x86_64.o: eightball.cc $(THIS_MAKE) | REL/lib64
	$(CXX) -o $@ -c $< $(RELEASE_x86_64_FLAGS) $(CXXFLAGS) -fPIC

REL/lib64/libeightball.so: REL/eightball_x86_64.o $(THIS_MAKE) | REL/lib64
	$(CXX) -o $@ $< $(RELEASE_x86_64_FLAGS) $(LDFLAGS) -shared

# Define rule for building NMF file and copying dependencies
REL_NEXES:=$(foreach src,$(NEXES),REL/$(src))

REL/$(PROJECT).nmf : $(REL_NEXES)
	cd REL && $(NMF) -o dlopen.nmf -s . $(NMF_PATHS) $(NEXES)

# Define a RELEASE alias to build the release version
.PHONY : RELEASE RUN_RELEASE
RELEASE : $(REL_NEXES) REL/$(PROJECT).nmf $(REL_COPIES)

# Define a RUN_RELEASE alias to build and server the RELEASE version
RUN_RELEASE: RELEASE
	cd REL && python ../../httpd.py
