# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# Simple Makefile.mk for demos
#
# Supported on Linux & Mac.
# Limited support under Cygwin on Windows (see note below)
#
# This makefile requires GNU make to run -- it is not compatible with nmake
# (and unlikely to be compatible with any other version of make.)
#
# The main purpose of this makefile component is to demonstrate building
# both nacl and non-nacl versions of some of the demos.  The non-nacl
# versions are regular executables that can be debugged using standard host
# debugging tools (gdb, KDbg, etc.)  The nacl versions (.nexe) require sel_ldr
# to execute, but the exact same nexe can be run on any nacl supported
# platform.
#
# This makefile component is intended to be included by a higher level
# Makefile after it has defined FILES and NAME.  Here are some examples
# of how to invoke from the higher level Makefile:
#
# make clean              Remove all built files
#
# make debug              Build standalone debug version using g++ compiler
# (default)               Output executable is $(NAME)_debug
#                         This executable can be debugged with gdb, kdbg, etc.
#                         Runnable on build platform only.
#
# make release            Build standalone release version using g++ compiler
#                         Output executable is $(NAME)
#                         Runnable on build platform only.
#
# make debug nacl         Build debug nacl version using nacl-g++
#                         Output executable is $(NAME)_debug.nexe
#                         Runnable on any nacl platform.
#
# make release nacl       Build release nacl version using nacl-g++
#                         Output executable is $(NAME).nexe
#                         Runnable on any nacl platform.
#
# make debug run          Build and run debug standalone build
#
# make release nacl run   Build and run release nacl version
#
# make release run ARGS="-w1024 -h1024 -m4"
#                         Build standalone release, and run it passing along
#                         command line arguments -w1024 -h1024 -m4
#
# on any build target, you can override optimization (OPT),
# add extra flags (CCFLAGS) add debug info (DBG) add extra
# libraries (LIBS) pass extra arguments onto the application
# when running it (ARGS) and (experimental) set optional platform
# (PLATFORM)  (default is autodetect between Mac & Linux)
#
# These overrides can be environment variables in your shell, passed
# in as arguments on the command line, or defined in Makefiles that
# include this file.
#
# Use the 'dbgldr' target to invoke the debug sel_ldr instead of
# the default optimized sel_ldr.
#
# For example, to override on the command line:
#
#   make debug OPT=-O2               build debug with -O2
#   make release DBG=-g              build release with debug info
#   make release nacl OPT=-O0        build release nacl with -O0
#   make release run ARGS="-m2"      build and run release passing "-m2"
#   make debug run ARGS="-w768 -h768" build and run debug with "-w768 -h768"
#   make release nacl run ARGS="-w1024 -h1024"
#
# Special Note for Cygwin on Windows: this makefile can be used
# to compile nacl versions of the demos, but does not support
# building them stand-alone.


CCFLAGS ?=
LIBS ?=
RUN ?=
ARGS ?=
SUBSYSTEM ?= multimedia
PRE_BUILD ?= @echo
POST_BUILD ?= @echo
SEL_LDR_ARGS ?= -d
NACL_CPP ?= nacl-g++
STANDALONE_CPP ?= g++
RELEASE_NAME := $(NAME)_release
DEBUG_NAME := $(NAME)_debug
NATIVE_CLIENT := $(subst /native_client,/native_client *,$(CURDIR))
NATIVE_CLIENT := $(filter-out *%,$(NATIVE_CLIENT))
GOOGLE_CLIENT := $(NATIVE_CLIENT)/..
NACL_BIN_PATH ?= \
     $(NATIVE_CLIENT)/src/third_party/nacl_sdk/$(PLATFORM)/sdk/nacl-sdk/bin
SEL_LDR ?= $(NATIVE_CLIENT)/scons-out/opt-$(PLATFORM)-x86-32/staging/sel_ldr
NACL_INCLUDE ?=
INCLUDE := -I$(GOOGLE_CLIENT) $(NACL_INCLUDE)

args:=$(MAKECMDGOALS)

# use debug sel_ldr...
ifeq (dbgldr,$(filter dbgldr,$(args)))
  SEL_LDR = $(NATIVE_CLIENT)/scons-out/dbg-$(PLATFORM)-x86-32/staging/sel_ldr
endif

# platform...
ifeq ($(shell uname -s), Darwin)
  PLATFORM ?= mac
  FRAMEWORKS_DIR ?= /Library/Frameworks
  SDL_FRAMEWORK_DIR ?= $(NATIVE_CLIENT)/src/third_party/sdl/osx/v1_2_13
  PLATFORM_SDL := -I$(SDL_FRAMEWORK_DIR)/SDL.framework/Headers
  PLATFORM_SDL += $(NATIVE_CLIENT)/common/SDLApplication.m
  PLATFORM_SDL += -F$(SDL_FRAMEWORK_DIR)
  PLATFORM_SDL += -framework SDL -framework Cocoa
else
  ENVIRON := $(shell uname -o)
  ifeq (Linux,$(findstring Linux,$(ENVIRON)))
    PLATFORM ?= linux
    SDL_FRAMEWORK_DIR ?= $(NATIVE_CLIENT)/src/third_party/sdl/linux/v1_2_13
    PLATFORM_SDL := -I$(SDL_FRAMEWORK_DIR)/include
    PLATFORM_SDL += -L$(SDL_FRAMEWORK_DIR)/lib
    PLATFORM_SDL += -lSDLmain -lSDL
  else
    ifeq (Cygwin,$(ENVIRON))
      PLATFORM ?= win
      PATH+= :$(NACL_BIN_PATH)
      SDL_FRAMEWORK_DIR ?= $(NATIVE_CLIENT)/src/third_party/sdl/win/v1_2_13
      NACL_BIN_PATH := \
          $(NATIVE_CLIENT)/src/third_party/nacl_sdk/windows/sdk/nacl-sdk/bin
      PLATFORM_SDL := -I$(SDL_FRAMEWORK_DIR)/include
      PLATFORM_SDL += -L$(SDL_FRAMEWORK_DIR)/lib
      PLATFORM_SDL += -lSDLmain -lSDL
      ifneq (nacl,$(filter nacl,$(args)))
        $(warning Stand-alone non-nacl builds are unsupported on Windows/Cygwin)
        $(error Please add 'nacl' target when invoking make)
      endif
    else
      $(error $(ENVIRON) is an unrecognized platform -- unable to continue)
    endif
  endif
endif

# process release vs debug options here
ifeq (release,$(filter release,$(args)))
  OPT ?= -O3 -mfpmath=sse -msse -fomit-frame-pointer
  DBG ?=
  OUTPUT_NAME :=$(RELEASE_NAME)
else
  # if release not specified, default to debug
  OPT ?= -O0
  DBG ?= -g
  OUTPUT_NAME := $(DEBUG_NAME)
endif

# xray profiler target
ifeq (xray,$(filter xray,$(MAKECMDGOALS)))
  XRAY_FILES := $(NATIVE_CLIENT)/tools/xray/xray.c
  XRAY_FILES += $(NATIVE_CLIENT)/tools/xray/stringpool.c
  XRAY_FILES += $(NATIVE_CLIENT)/tools/xray/hashtable.c
  XRAY_FILES += $(NATIVE_CLIENT)/tools/xray/symtable.c
  XRAY_OPT := -O2 -finstrument-functions -falign-functions=32
  XRAY_OPT += -DXRAY -DXRAY_ANNOTATE
  XRAY_OPT += -I$(NATIVE_CLIENT)/tools/xray
  XRAY_OPT += -Wl,-Map,$(NAME).map
  XRAY_POST_BUILD := c++filt <$(NAME).map >$(NAME).map2 ;
  XRAY_POST_BUILD += mv $(NAME).map2 $(NAME).map
else
  XRAY_FILES :=
  XRAY_OPT :=
  XRAY_POST_BUILD :=
endif

# process nacl vs non-nacl options here
ifeq (nacl,$(filter nacl,$(args)))
  CPP := $(NACL_BIN_PATH)/$(NACL_CPP)
  EXE_NAME := $(OUTPUT_NAME).nexe
  # args to nacl app need "--" prefix
  ifdef ARGS
    APP_ARGS := -- $(ARGS)
  else
    APP_ARGS :=
  endif
  MULTIMEDIA :=
  APP_FILES := $(FILES)
  RUN := $(SEL_LDR) $(SEL_LDR_ARGS) -f $(EXE_NAME)
  OBJDUMP = $(NACL_BIN_PATH)/nacl-objdump
  LIBS += -lav -lsrpc
else
  CPP := $(STANDALONE_CPP)
  CCFLAGS += -DSTANDALONE
  EXE_NAME := $(OUTPUT_NAME)
  ifeq ($(SUBSYSTEM),multimedia)
    MULTIMEDIA := $(PLATFORM_SDL)
    APP_FILES := $(FILES) $(NATIVE_CLIENT)/common/standalone.cc
  else
    MULTIMEDIA :=
    APP_FILES := $(FILES)
  endif
  APP_ARGS := $(ARGS)
  RUN := ./$(EXE_NAME)
  OBJDUMP = objdump
endif

SRC_FILES := $(APP_FILES) $(XRAY_FILES)

# include common libs that most demos want
LIBS += -lpthread -lm

# build up all the various options
OPTIONS:=$(CCFLAGS) $(OPT) $(XRAY_OPT) $(DBG) $(LIBS)

# targets

debug: $(EXE_NAME)

release: $(EXE_NAME)

info:
	@echo -----------------
	@echo args: $(args)
	@echo PATH: $(PATH)
	@echo CURDIR: $(CURDIR)
	@echo GOOGLE_CLIENT: $(GOOGLE_CLIENT)
	@echo NATIVE_CLIENT: $(NATIVE_CLIENT)
	@echo CCFLAGS: $(CCFLAGS)
	@echo DBG: $(DBG)
	@echo OPT: $(OPT)
	@echo APP_FILES: $(APP_FILES)
	@echo SRC_FILES: $(SRC_FILES)
	@echo OPTIONS: $(OPTIONS)
	@echo LIBS: $(LIBS)
	@echo PLATFORM_SDL: $(PLATFORM_SDL)
	@echo INCLUDE: $(INCLUDE)
	@echo EXE_NAME: $(EXE_NAME)
	@echo SEL_LDR: $(SEL_LDR)
	@echo CPP: $(CPP)
	@echo PRE_BUILD: $(PRE_BUILD)
	@echo POST_BUILD: $(POST_BUILD)
	@echo XRAY_OPT: $(XRAY_OPT)
	@echo XRAY_FILES: $(XRAY_FILES)
	@echo XRAY_POST_BUILD: $(XRAY_POST_BUILD)
	@echo NACL_BIN_PATH: $(NACL_BIN_PATH)
	@echo -----------------

nacl:
	@echo make: Native Client target specified

clean:
	rm -f *.o *.nexe $(DEBUG_NAME) $(RELEASE_NAME)
	rm -rf $(DEBUG_NAME).dSYM

$(EXE_NAME): $(SRC_FILES)
	$(PRE_BUILD)
	$(CPP) $(SRC_FILES) $(INCLUDE) $(OPTIONS) -o $(EXE_NAME) $(MULTIMEDIA)
	$(POST_BUILD)
	$(XRAY_POST_BUILD)

dbgldr:
	@echo make: Using debug build of sel_ldr

run:
	$(RUN) $(APP_ARGS)

xray:
	@echo make: XRay Profiler target specified

.PHONY: debug release clean nacl run xray info dbgldr
