# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
# http://www.gnu.org/software/make/manual/make.html
#

#
# Get pepper directory for toolchain and includes.
#
# If NACL_SDK_ROOT is not set, then assume it can be found three directories up.
#
THIS_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
NACL_SDK_ROOT ?= $(abspath $(dir $(THIS_MAKEFILE))../..)

# Project Build flags
WARNINGS := -Wno-long-long -Wall -Wswitch-enum -pedantic -Werror
CXXFLAGS := -pthread -std=gnu++98 $(WARNINGS)

#
# Compute tool paths
#
GETOS := python $(NACL_SDK_ROOT)/tools/getos.py
OSHELPERS = python $(NACL_SDK_ROOT)/tools/oshelpers.py
OSNAME := $(shell $(GETOS))
RM := $(OSHELPERS) rm

PNACL_TC_PATH := $(abspath $(NACL_SDK_ROOT)/toolchain/$(OSNAME)_pnacl)
PNACL_CXX := $(PNACL_TC_PATH)/newlib/bin/pnacl-clang++
PNACL_FINALIZE := $(PNACL_TC_PATH)/newlib/bin/pnacl-finalize
CXXFLAGS := -I$(NACL_SDK_ROOT)/include
LDFLAGS := -L$(NACL_SDK_ROOT)/lib/pnacl/Release -lppapi_cpp -lppapi

#
# Disable DOS PATH warning when using Cygwin based tools Windows
#
CYGWIN ?= nodosfilewarning
export CYGWIN


# Declare the ALL target first, to make the 'all' target the default build
all: hello_tutorial.pexe

clean:
	$(RM) hello_tutorial.pexe hello_tutorial.bc

hello_tutorial.bc: hello_tutorial.cc
	$(PNACL_CXX) -o $@ $< -O2 $(CXXFLAGS) $(LDFLAGS)

hello_tutorial.pexe: hello_tutorial.bc
	$(PNACL_FINALIZE) -o $@ $<


#
# File to redirect to to in order to hide output.
#
ifeq ($(OSNAME),win)
DEV_NULL = nul
else
DEV_NULL = /dev/null
endif

#
# Assign a sensible default to CHROME_PATH.
#
CHROME_PATH ?= $(shell $(GETOS) --chrome 2> $(DEV_NULL))

#
# Verify we can find the Chrome executable if we need to launch it.
#
.PHONY: check_for_chrome
check_for_chrome:
ifeq (,$(wildcard $(CHROME_PATH)))
	$(warning No valid Chrome found at CHROME_PATH=$(CHROME_PATH))
	$(error Set CHROME_PATH via an environment variable, or command-line.)
else
	$(warning Using chrome at: $(CHROME_PATH))
endif

#
# Variables for running examples with Chrome.
#
RUN_PY := python $(NACL_SDK_ROOT)/tools/run.py

# Additional arguments to pass to Chrome.
CHROME_ARGS += --enable-pnacl --no-first-run
CHROME_ARGS += --user-data-dir=$(CURDIR)/user-data-dir

# Define a phony rule so it always runs, to run the pexe.
.PHONY: run
run: check_for_chrome all
	$(RUN_PY) -C $(CURDIR) -P index.html -- $(CHROME_PATH) $(CHROME_ARGS)
