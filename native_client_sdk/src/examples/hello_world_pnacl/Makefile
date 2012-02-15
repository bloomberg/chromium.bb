# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
#   http://www.gnu.org/software/make/manual/make.html
#


#
# Get pepper directory for toolchain and includes.
#
# If NACL_SDK_ROOT is not set, then assume it can be found a two directories up,
# from the default example directory location.
#
THIS_MAKEFILE:=$(abspath $(lastword $(MAKEFILE_LIST)))
NACL_SDK_ROOT?=$(abspath $(dir $(THIS_MAKEFILE))../..)


#
# Project Build flags
#
# Turns on warnings (-Wxxx), builds with zero optimization (-O0) and adds debug
# information (-g) for correctness and ease of debugging.
WARNINGS:=-Wno-long-long -Wall -Wswitch-enum -Werror -pedantic 
CFLAGS:=-pthread -O0 -g $(WARNINGS)


#
# Compute path to compiler
#
OSNAME:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py)
TC_PATH:=$(abspath $(NACL_SDK_ROOT)/toolchain/$(OSNAME)_x86_pnacl)


# Alias for C/C++ compiler and translater
CC:=$(TC_PATH)/newlib/bin/pnacl-clang
TRANSLATE:=$(TC_PATH)/newlib/bin/pnacl-translate

#
# Disable DOS PATH warning when using Cygwin based tools Windows
#
CYGWIN ?= nodosfilewarning
export CYGWIN

# Default target is everything
all : hello_world_x86_32.nexe hello_world_x86_64.nexe hello_world_arm.nexe

# Define compile and link rule for portable NEXE format
hello_world.pexe : hello_world.c $(THIS_MAKE)
	$(CC) -o $@ $< -O0 -g $(CFLAGS) -lppapi

# Define translation rules
hello_world_x86_32.nexe : hello_world.pexe
	$(TRANSLATE) -arch x86-32 $< -o $@ 

hello_world_x86_64.nexe : hello_world.pexe
	$(TRANSLATE) -arch x86-64 $< -o $@

hello_world_arm.nexe : hello_world.pexe
	$(TRANSLATE) -arch arm $< -o $@

# Define a phony rule so it always runs, to build nexe and start up server.
.PHONY: RUN 
RUN: all
	python ../httpd.py
