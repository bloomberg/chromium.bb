# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
#   http://www.gnu.org/software/make/manual/make.html
#
#

#
# Toolchain
#
# This makefile is designed to work with the NEWLIB toolchain which is
# currently supported by x86 and ARM.  To switch to glibc, you would need
# to drop support for ARM.
#
TOOLCHAIN?=newlib

#
# Build Configuration
#
# The SDK provides two sets of libraries, Debug and Release.  Debug libraries
# are compiled without optimizations to make debugging easier.  By default
# this will build a Debug configuration.
#
CONFIG?=Debug


# Note for Windows:
#   Both GCC and LLVM bases tools (include the version of Make.exe that comes
# with the SDK) both expect and are capable of dealing with the '/' seperator.
# For that reason, the tools in the SDK, including build, compilers, scripts
# all have a preference for POSIX style command-line arguments.
#
# Keep in mind however that the shell is responsible for command-line escaping,
# globbing, and variable expansion, so those may change based on which shell
# is used.  For Cygwin shells this can include automatic and incorrect expansion
# of response files (files starting with '@').
#

# Disable DOS PATH warning when using Cygwin based NaCl tools on Windows.
#
CYGWIN?=nodosfilewarning
export CYGWIN


#
# Get pepper directory for toolchain and includes.
#
# If NACL_SDK_ROOT is not set, then assume it can be found a two directories up,
# from the default example directory location.
#
THIS_MAKEFILE:=$(abspath $(lastword $(MAKEFILE_LIST)))
THIS_DIR:=$(abspath $(dir $(THIS_MAKEFILE)))
NACL_SDK_ROOT?=$(abspath $(dir $(THIS_MAKEFILE))../..)


#
# Defaults build flags
#
# Convert warnings to errors, and build with no optimization.
#
NACL_WARNINGS:=-Wno-long-long -Werror
OPT_FLAGS:=-g -O0
CXX_FLAGS:=-pthread -I$(NACL_SDK_ROOT)/include
LD_FLAGS:=-pthread


#
# Library Paths
#
# Libraries are stored in different directories for each achitecture as well
# as different subdirectories for Debug vs Release configurations.  This make
# only supports the Debug configuration for simplicity.
#
# By default for x86 32 bit this expands to:
#   $(NACL_SDK_ROOT)/lib/newlib_x86_32/Debug
#
LD_X86_32:=-L$(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_x86_32/$(CONFIG)
LD_X86_64:=-L$(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_x86_64/$(CONFIG)
LD_ARM:=-L$(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_arm/$(CONFIG)


#
# Compute path to requested NaCl Toolchain
#
OSNAME:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py)
TC_PATH:=$(abspath $(NACL_SDK_ROOT)/toolchain)


#
# Alias for standard POSIX file system commands
#
CP:=python $(NACL_SDK_ROOT)/tools/oshelpers.py cp
MKDIR:=python $(NACL_SDK_ROOT)/tools/oshelpers.py mkdir
MV:=python $(NACL_SDK_ROOT)/tools/oshelpers.py mv
RM:=python $(NACL_SDK_ROOT)/tools/oshelpers.py rm


#
# The default target
#
# If no targets are specified on the command-line, the first target listed in
# the makefile becomes the default target.  By convention this is usually called
# the 'all' target.  Here we leave it blank to be first, but define it later
#
all:


#
# Target to remove temporary files
#
.PHONY: clean
clean:
	$(RM) $(TARGET).nmf
	$(RM) -fr $(TOOLCHAIN)

#
# Macros for TOOLS
#
# We use the C++ compiler for everything and then use the -Wl,-as-needed flag
# in the linker to drop libc++ unless it's actually needed.
#
X86_CXX?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/bin/i686-nacl-g++
X86_LINK?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/bin/i686-nacl-g++ -Wl,-as-needed

ARM_CXX?=$(TC_PATH)/$(OSNAME)_arm_$(TOOLCHAIN)/bin/arm-nacl-g++
ARM_LINK?=$(TC_PATH)/$(OSNAME)_arm_$(TOOLCHAIN)/bin/arm-nacl-g++ -Wl,-as-needed

PNACL_CXX?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/newlib/bin/pnacl-clang++ -c
PNACL_LINK?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/newlib/bin/pnacl-clang++


#
# Rules for output directories.
#
# Output will be places in a directory name based on Toolchain and configuration
# be default this will be "newlib/Debug".  We use a python wrapped MKDIR to
# proivde a cross platform solution. The use of '|' checks for existance instead
# of timestamp, since the directory can update when files change.
#
$(TOOLCHAIN):
	$(MKDIR) $(TOOLCHAIN)

$(TOOLCHAIN)/$(CONFIG): | $(TOOLCHAIN)
	$(MKDIR) $(TOOLCHAIN)/$(CONFIG)

OUTDIR:=$(TOOLCHAIN)/$(CONFIG)


#
# Dependency Macro
#
# $1 = Name of dependency
#
define DEPEND_RULE
.PHONY: $(1)
$(1):
	+$(MAKE) -C $(NACL_SDK_ROOT)/src/$(1)
DEPS_LIST+=$(1)
endef

#
# Compile Macro
#
# $1 = Source Name
#
# By default, if $(1) = source.c, this rule expands to:
#    newlib/Debug/source_x86_32.o : souce.c Makefile | newlib/Debug
#
# Which means if 'source.c' or Makefile are newer than the object
# newlib/Debug/source_x86_32.o, then run the step:
#      $(X86_CC) -o newlib/Debug/source_x86_32.o -c source.c ....
#
# We repeat this expansion for 64 bit X86 and conditionally for ARM if
# TOOLCHAIN=newlib
#
define COMPILE_RULE
$(OUTDIR)/$(basename $(1))_x86_32.o : $(1) $(THIS_MAKE) | $(OUTDIR)
	$(X86_CXX) -o $$@ -c $$< -m32 $(OPT_FLAGS) $(CXX_FLAGS) $(NACL_WARNINGS)

$(OUTDIR)/$(basename $(1))_x86_64.o : $(1) $(THIS_MAKE) | $(OUTDIR)
	$(X86_CXX) -o $$@ -c $$< -m64 $(OPT_FLAGS) $(CXX_FLAGS) $(NACL_WARNINGS)

$(OUTDIR)/$(basename $(1))_arm.o : $(1) $(THIS_MAKE) | $(OUTDIR)
	$(ARM_CXX) -o $$@ -c $$< $(OPT_FLAGS) $(CXX_FLAGS) $(NACL_WARNINGS)

$(OUTDIR)/$(basename $(1))_pnacl.o : $(1) $(THIS_MAKE) | $(OUTDIR)
	$(PNACL_CXX) -o $$@ -c $$< $(OPT_FLAGS) $(CXX_FLAGS) $(NACL_WARNINGS)
endef


#
# Link Macro
#
# $1 = Target Name
# $2 = List of Sources
#
# By default, if $(1) = foo $(2) = A.c B.cc, this rule expands to:
#   newlib/Debug/foo_x86_32.nexe : newlib/Debug/A_x86_32.o ...
#
# Which means if A_x86_32.o or sourceB_32.o is newer than the nexe then
# run the build step:
#   $(X86_LINK) -o newlib/Debug/foo_x86_32.nexe newlib/Debug/A_x86_32.o ...
#
# Note:
#   We expand each library as '-l<name>' which will look for lib<name> in the
# directory specified by $(LD_X86_32)
#
# We repeat this expansion for 64 bit X86 and conditionally for ARM if
# TOOLCHAIN=newlib
#
define LINK_RULE
NMF_TARGETS+=$(OUTDIR)/$(1)_x86_32.nexe
$(OUTDIR)/$(1)_x86_32.nexe : $(foreach src,$(2),$(OUTDIR)/$(basename $(src))_x86_32.o)
	$(X86_LINK) -o $$@ $$^ -m32 $(LD_X86_32) $(LD_FLAGS) $(foreach lib,$(LIBS),-l$(lib))

NMF_TARGETS+=$(OUTDIR)/$(1)_x86_64.nexe
$(OUTDIR)/$(1)_x86_64.nexe : $(foreach src,$(2),$(OUTDIR)/$(basename $(src))_x86_64.o)
	$(X86_LINK) -o $$@ $$^ -m64 $(LD_X86_64) $(LD_FLAGS) $(foreach lib,$(LIBS),-l$(lib))

NMF_TARGETS+=$(OUTDIR)/$(1)_arm.nexe
$(OUTDIR)/$(1)_arm.nexe : $(foreach src,$(2),$(OUTDIR)/$(basename $(src))_arm.o)
	$(ARM_LINK) -o $$@ $$^ $(LD_ARM) $(LD_FLAGS) $(foreach lib,$(LIBS),-l$(lib))

NMF_TARGETS+=$(OUTDIR)/$(1).pexe
$(OUTDIR)/$(1).pexe : $(foreach src,$(2),$(OUTDIR)/$(basename $(src))_pnacl.o)
	$(PNACL_LINK) -o $$@ $$^ $(LD_PNACL) $(LD_FLAGS) $(foreach lib,$(LIBS),-l$(lib))
endef



#
# Generate NMF_TARGETS
#
ARCHES=x86_32 x86_64
ifeq "newlib" "$(TOOLCHAIN)"
ARCHES+=arm
endif
NMF_ARCHES:=$(foreach arch,$(ARCHES),_$(arch).nexe)

ifeq "pnacl" "$(TOOLCHAIN)"
NMF_ARCHES:=.pexe
endif


#
# NMF Manifiest generation
#
# Use the python script create_nmf to scan the binaries for dependencies using
# objdump.  Pass in the (-L) paths to the default library toolchains so that we
# can find those libraries and have it automatically copy the files (-s) to
# the target directory for us.
#
# $1 = Target Name (the basename of the nmf
# $2 = Additional create_nmf.py arguments
#
NMF:=python $(NACL_SDK_ROOT)/tools/create_nmf.py
GLIBC_DUMP:=$(TC_PATH)/$(OSNAME)_x86_glibc/x86_64-nacl/bin/objdump
GLIBC_PATHS:=-L $(TC_PATH)/$(OSNAME)_x86_glibc/x86_64-nacl/lib32
GLIBC_PATHS+=-L $(TC_PATH)/$(OSNAME)_x86_glibc/x86_64-nacl/lib

define NMF_RULE
$(OUTDIR)/$(1).nmf : $(foreach arch,$(NMF_ARCHES),$(OUTDIR)/$(1)$(arch))
	$(NMF) -o $$@ $$^ -D $(GLIBC_DUMP) $(GLIBC_PATHS) -s $(OUTDIR) $(2)

all : $(DEPS_LIST) $(OUTDIR)/$(1).nmf
endef


#
# Verify we can find the Chrome executable if we need to launch it.
#
.PHONY: CHECK_FOR_CHROME RUN LAUNCH
CHECK_FOR_CHROME:
ifeq (,$(wildcard $(CHROME_PATH)))
	$(warning No valid Chrome found at CHROME_PATH=$(CHROME_PATH))
	$(error Set CHROME_PATH via an environment variable, or command-line.)
else
	$(warning Using chrome at: $(CHROME_PATH))
endif


#
# Variables for running examples with Chrome.
#
RUN_PY:=python $(NACL_SDK_ROOT)/tools/run.py

# Add this to launch Chrome with additional environment variables defined.
# Each element should be specified as KEY=VALUE, with whitespace separating
# key-value pairs. e.g.
# CHROME_ENV=FOO=1 BAR=2 BAZ=3
CHROME_ENV?=

# Additional arguments to pass to Chrome.
CHROME_ARGS+=--enable-nacl --enable-pnacl --incognito --ppapi-out-of-process


CONFIG?=Debug
PAGE?=index_$(TOOLCHAIN)_$(CONFIG).html

RUN: LAUNCH
LAUNCH: CHECK_FOR_CHROME all
ifeq (,$(wildcard $(PAGE)))
	$(warning No valid HTML page found at $(PAGE))
	$(error Make sure TOOLCHAIN and CONFIG are properly set)
endif
	$(RUN_PY) -C $(THIS_DIR) -P $(PAGE) $(addprefix -E ,$(CHROME_ENV)) -- \
	    $(CHROME_PATH) $(CHROME_ARGS) \
	    --register-pepper-plugins="$(PPAPI_DEBUG),$(PPAPI_RELEASE)"

