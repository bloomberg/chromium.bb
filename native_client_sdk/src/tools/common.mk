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
VALID_TOOLCHAINS?=newlib
TOOLCHAIN?=$(word 1,$(VALID_TOOLCHAINS))


#
# Top Make file, which we want to trigger a rebuild on if it changes
#
TOP_MAKE:=$(word 1,$(MAKEFILE_LIST))


#
# Verify we selected a valid toolchain for this example
#
ifeq (,$(findstring $(TOOLCHAIN),$(VALID_TOOLCHAINS)))
$(warning Availbile choices are: $(VALID_TOOLCHAINS))
$(error Can not use TOOLCHAIN=$(TOOLCHAIN) on this example.)
endif


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
# Alias for standard POSIX file system commands
#
CP:=python $(NACL_SDK_ROOT)/tools/oshelpers.py cp
MKDIR:=python $(NACL_SDK_ROOT)/tools/oshelpers.py mkdir
MV:=python $(NACL_SDK_ROOT)/tools/oshelpers.py mv
RM:=python $(NACL_SDK_ROOT)/tools/oshelpers.py rm


#
# Compute path to requested NaCl Toolchain
#
OSNAME:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py)
TC_PATH:=$(abspath $(NACL_SDK_ROOT)/toolchain)




#
# The default target
#
# If no targets are specified on the command-line, the first target listed in
# the makefile becomes the default target.  By convention this is usually called
# the 'all' target.  Here we leave it blank to be first, but define it later
#
all:


#
# Target a toolchain
#
# $1 = Toolchain Name
#
define TOOLCHAIN_RULE
.PHONY: all_$(1)
all_$(1):
	+$(MAKE) TOOLCHAIN=$(1)
TOOLCHAIN_LIST+=all_$(1)
endef


#
# The target for all versions
#
USABLE_TOOLCHAINS=$(filter $(OSNAME) newlib glibc pnacl,$(VALID_TOOLCHAINS))
$(foreach tool,$(USABLE_TOOLCHAINS),$(eval $(call TOOLCHAIN_RULE,$(tool),$(dep))))
all_versions: $(TOOLCHAIN_LIST)

#
# Target to remove temporary files
#
.PHONY: clean
clean:
	$(RM) $(TARGET).nmf
	$(RM) -fr $(TOOLCHAIN)


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
-include $(OUTDIR)/*.d


#
# Dependency Macro
#
# $1 = Name of dependency
#
define DEPEND_RULE
.PHONY: $(1)
$(1):
ifeq (,$(IGNORE_DEPS))
	@echo "Checking library: $(1)"
	+$(MAKE) -C $(NACL_SDK_ROOT)/src/$(1)
DEPS_LIST+=$(1)
else
	@echo "Ignore DEPS: $(1)"
endif
endef


ifeq ('win','$(TOOLCHAIN)')
HOST_EXT=.dll
else
HOST_EXT=.so
endif


#
# Common Compile Options
#
ifeq ('Release','$(CONFIG)')
POSIX_OPT_FLAGS?=-g -O2 -pthread
else
POSIX_OPT_FLAGS?=-g -O0 -pthread
endif

NACL_CFLAGS?=-Wno-long-long -Werror
NACL_CXXFLAGS?=-Wno-long-long -Werror

#
# Default Paths
#
ifeq (,$(findstring $(TOOLCHAIN),linux mac win))
INC_PATHS?=$(NACL_SDK_ROOT)/include $(EXTRA_INC_PATHS)
else
INC_PATHS?=$(NACL_SDK_ROOT)/include/$(OSNAME) $(NACL_SDK_ROOT)/include $(EXTRA_INC_PATHS)
endif

LIB_PATHS?=$(NACL_SDK_ROOT)/lib $(EXTRA_LIB_PATHS)


#
# If the requested toolchain is a NaCl or PNaCl toolchain, the use the
# macros and targets defined in nacl.mk, otherwise use the host sepecific
# macros and targets.
#
ifneq (,$(findstring $(TOOLCHAIN),linux mac))
include $(NACL_SDK_ROOT)/tools/host_gcc.mk
endif

ifneq (,$(findstring $(TOOLCHAIN),win))
include $(NACL_SDK_ROOT)/tools/host_vc.mk
endif

ifneq (,$(findstring $(TOOLCHAIN),glibc newlib))
include $(NACL_SDK_ROOT)/tools/nacl_gcc.mk
endif

ifneq (,$(findstring $(TOOLCHAIN),pnacl))
include $(NACL_SDK_ROOT)/tools/nacl_llvm.mk
endif


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


# Paths to Debug and Release versions of the Host Pepper plugins
PPAPI_DEBUG=$(abspath $(OSNAME)/Debug/$(TARGET)$(HOST_EXT));application/x-ppapi-debug
PPAPI_RELEASE=$(abspath $(OSNAME)/Release/$(TARGET)$(HOST_EXT));application/x-ppapi-release

info:
	@echo "DEBUG=$(PPAPI_DEBUG)"

PAGE?=index_$(TOOLCHAIN)_$(CONFIG).html

RUN: LAUNCH
LAUNCH: CHECK_FOR_CHROME all
ifeq (,$(wildcard $(PAGE)))
	$(warning No valid HTML page found at $(PAGE))
	$(error Make sure TOOLCHAIN and CONFIG are properly set)
endif
	$(RUN_PY) -C $(CURDIR) -P $(PAGE) $(addprefix -E ,$(CHROME_ENV)) -- \
	    $(CHROME_PATH) $(CHROME_ARGS) \
	    --register-pepper-plugins="$(PPAPI_DEBUG),$(PPAPI_RELEASE)"

