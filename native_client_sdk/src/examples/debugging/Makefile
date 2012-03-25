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
CHROME_PATH?=Undefined

#
# Project Build flags
#
# Turns on warnings (-Wxxx), builds with zero optimization (-O0) and adds debug
# information (-g) for correctness and ease of debugging.
WARNINGS:=-Wno-long-long -Wall -Wswitch-enum -Werror -pedantic 
CFLAGS:=-pthread -O0 -g $(WARNINGS) -fno-omit-frame-pointer
LDFLAGS:=-pthread -lppapi
CC_SOURCES:=hello_world.c untrusted_crash_dump.c string_stream.c
PROJECT:=hello_world

#
# Compute path to compiler
#
OSNAME:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py)
TC_PATH:=$(abspath $(NACL_SDK_ROOT)/toolchain/$(OSNAME)_x86_newlib)

#
# Verify we have a valid NACL_SDK_ROOT
#
ifeq (,$(wildcard $(TC_PATH)))
$(warning No valid NACL_SDK_ROOT at $(NACL_SDK_ROOT))
ifeq ($(origin NACL_SDK_ROOT), 'file')
$(error Override the default value via enviornment variable, or command-line.)
else
$(error Fix the NACL_SDK_ROOT specified in the environment or command-line.)
endif
endif

# Alias for C compiler
CC:=$(TC_PATH)/bin/i686-nacl-gcc


#
# Disable DOS PATH warning when using Cygwin based tools Windows
#
CYGWIN ?= nodosfilewarning
export CYGWIN


# Default target is everything
all : hello_world_x86_32.nexe hello_world_x86_64.nexe

# Define 32 bit compile and link rules for C sources
x86_32_OBJS:=$(patsubst %.c,%_32.o,$(CC_SOURCES))
$(x86_32_OBJS) : %_32.o : %.c $(THIS_MAKE)
	$(CC) -o $@ -c $< -m32 -O0 -g $(CCFLAGS)

$(PROJECT)_x86_32.nexe : $(x86_32_OBJS)
	$(CC) -o $@ $^ -m32 -O0 -g $(CCFLAGS) $(LDFLAGS)

# Define 64 bit compile and link rules for C sources
x86_64_OBJS:=$(patsubst %.c,%_64.o,$(CC_SOURCES))
$(x86_64_OBJS) : %_64.o : %.c $(THIS_MAKE)
	$(CC) -o $@ -c $< -m64 -O0 -g $(CCFLAGS)

$(PROJECT)_x86_64.nexe : $(x86_64_OBJS)
	$(CC) -o $@ $^ -m64 -O0 -g $(CCFLAGS) $(LDFLAGS)

# Define a phony rule so it always runs, to build nexe and start up server.
.PHONY: RUN 
RUN: all
	python ../httpd.py


#
# Setup environment to use SDK's version of the NaCl plugin, instead of the
# one built into Chrome.  This requries launching Chrome which means we must
# be able to find it.  It also means that we must determine which versions
# of the plugin, loader, irt, and any other helpers we must use.
#
HELPER_PATH:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py --helper)
IRTBIN_PATH:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py --irtbin)
LOADER_PATH:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py --loader)
PLUGIN_PATH:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py --plugin)
CHROME_ARGS:=--incognito --no-sandbox
CHROME_ARGS+="--register-pepper-plugins=$(PLUGIN_PATH);application/x-nacl"
CHROME_ARGS+="localhost:5103/debugging.html"


.PHONY: SETUP_FOR_CHROME
SETUP_FOR_CHROME:
ifeq (,$(wildcard $(CHROME_PATH)))
	$(warning No valid Chrome found at CHROME_PATH=$(CHROME_PATH))
	$(error Set CHROME_PATH via an environment variable, or command-line.)
endif


ifneq (,$(wildcard $(CHROME_PATH)))
export NACL_DANGEROUS_ENABLE_FILE_ACCESS=1
export NACL_SECURITY_DISABLE=1
export NACL_UNTRUSTED_EXCEPTION_HANDLING=1
export NACL_SEL_LDR=$(LOADER_PATH)
export NACL_IRT_LIBRARY=$(IRTBIN_PATH)
export NACL_SEL_LDR_BOOTSTRAP=$(HELPER_PATH)
endif


TRACE: SETUP_FOR_CHROME all
	$(CHROME_PATH) $(CHROME_ARGS)
