# Copyright (c) 2012 The Native Client Authors. All rights reserved.
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
WARNINGS:=-Wno-long-long -Wall
CXXFLAGS:=-g -O0 -pthread -std=gnu++98 $(WARNINGS)
LDFLAGS:=-g -ldl -lppapi_cpp -lppapi


#
# Get pepper directory for toolchain and includes.
#
# If PEPPER_ROOT is not set, then assume it can be found a two directories up,
# from the default example directory location.
#
THIS_MAKEFILE:=$(abspath $(lastword $(MAKEFILE_LIST)))
NACL_SDK_ROOT?=$(abspath $(dir $(THIS_MAKEFILE))../..)

#
# Compute tool paths
#
#
OSNAME:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py)
TC_PATH:=$(abspath $(NACL_SDK_ROOT)/toolchain/$(OSNAME)_x86_glibc)
CXX:=$(TC_PATH)/bin/i686-nacl-g++
NMF:=python $(NACL_SDK_ROOT)/tools/create_nmf.py

#
# Create shell aliases
#
# Create Python based aliases for common shell commands like copy or move.
#
COPY:= python $(NACL_SDK_ROOT)/tools/oshelpers.py cp
MKDIR:= python $(NACL_SDK_ROOT)/tools/oshelpers.py mkdir
RM:= python $(NACL_SDK_ROOT)/tools/oshelpers.py rm
MV:= python $(NACL_SDK_ROOT)/tools/oshelpers.py mv

#
# Disable DOS PATH warning when using Cygwin based tools Windows
#
CYGWIN ?= nodosfilewarning
export CYGWIN


#
# NMF Manifiest generation
#
NMF:=python $(NACL_SDK_ROOT)/tools/create_nmf.py
NMF+=-D $(TC_PATH)/x86_64-nacl/bin/objdump
NMF_PATHS:=-L $(TC_PATH)/x86_64-nacl/lib32 -L $(TC_PATH)/x86_64-nacl/lib
NMF_PATHS+=-L lib32 -L lib64


#
# Disable DOS PATH warning when using Cygwin based tools Windows
#
CYGWIN ?= nodosfilewarning
export CYGWIN


# Declare the ALL target first, to make the 'all' target the default build.
# Since the NMF file requires all the binaires for generation we use that as
# the dependency.
all : $(PROJECT).nmf

# Rules to create subdirectories for libraries
lib32:
	$(MKDIR) -p $@

lib64:
	$(MKDIR) -p $@

# Copy all files to that config
$(foreach src,$(COPY_FILES),$(eval $(call FILE_COPY,$(src),DBG)))

# Build debug version dlopen nexe and eightball.so for 32 and 64 bit.
dlopen_x86_32.o: dlopen.cc $(THIS_MAKE)
	$(CXX) -o $@ -c $< -m32 $(CXXFLAGS)

dlopen_x86_32.nexe: dlopen_x86_32.o
	$(CXX) -o $@ $< -m32 $(LDFLAGS)

dlopen_x86_64.o: dlopen.cc $(THIS_MAKE)
	$(CXX) -o $@ -c $< -m64 $(CXXFLAGS)

dlopen_x86_64.nexe: dlopen_x86_64.o
	$(CXX) -o $@ $< -m64 $(LDFLAGS)

eightball_x86_32.o: eightball.cc $(THIS_MAKE)
	$(CXX) -o $@ -c $< -m32 $(CXXFLAGS) -fPIC

lib32/libeightball.so: eightball_x86_32.o | lib32
	$(CXX) -o $@ $< -m32 $(LDFLAGS) -shared

eightball_x86_64.o: eightball.cc $(THIS_MAKE)
	$(CXX) -o $@ -c $< -m64 $(CXXFLAGS) -fPIC

lib64/libeightball.so: eightball_x86_64.o | lib64
	$(CXX) -o $@ $< -m64 $(LDFLAGS) -shared

#
# NMF Manifiest generation
#
# Use the python script create_nmf to scan the binaries for dependencies using
# objdump.  Pass in the (-L) paths to the default library toolchains so that we
# can find those libraries and have it automatically copy the files (-s) to
# the target directory for us.
NEXES:=dlopen_x86_32.nexe dlopen_x86_64.nexe
NEXES+=lib32/libeightball.so lib64/libeightball.so
NMF_ARGS:=-D $(TC_PATH)/x86_64-nacl/bin/objdump
NMF_PATHS:=-L $(TC_PATH)/x86_64-nacl/lib32 -L $(TC_PATH)/x86_64-nacl/lib64

$(PROJECT).nmf : $(NEXES)
	$(NMF) -o $@ -s . $^ $(NMF_PATHS)

# Define a phony rule so it always runs, to build nexe and start up server.
.PHONY: RUN 
RUN: all
	python ../httpd.py

