# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
#   http://www.gnu.org/software/make/manual/make.html
#
#


#
# Default Paths
#

LD_X86_32:=-L$(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_x86_32/$(CONFIG)
LD_X86_64:=-L$(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_x86_64/$(CONFIG)
LD_ARM:=-L$(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_arm/$(CONFIG)


#
# Macros for TOOLS
#
# We always link with the C++ compiler but include -Wl,-as-needed flag
# in LD_FLAGS so the linker should drop libc++ unless it's actually needed.
#
X86_32_CC?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/bin/i686-nacl-gcc
X86_32_CXX?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/bin/i686-nacl-g++
X86_32_LINK?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/bin/i686-nacl-g++
X86_32_LIB?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/bin/i686-nacl-ar r

X86_64_CC?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/bin/x86_64-nacl-gcc
X86_64_CXX?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/bin/x86_64-nacl-g++
X86_64_LINK?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/bin/x86_64-nacl-g++
X86_64_LIB?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/bin/x86_64-nacl-ar r

ARM_CC?=$(TC_PATH)/$(OSNAME)_arm_$(TOOLCHAIN)/bin/arm-nacl-gcc
ARM_CXX?=$(TC_PATH)/$(OSNAME)_arm_$(TOOLCHAIN)/bin/arm-nacl-g++
ARM_LINK?=$(TC_PATH)/$(OSNAME)_arm_$(TOOLCHAIN)/bin/arm-nacl-g++
ARM_LIB?=$(TC_PATH)/$(OSNAME)_arm_$(TOOLCHAIN)/bin/arm-nacl-ar r


#
# Compile Macro
#
# $1 = Source Name
# $2 = Compile Flags
#

define C_COMPILER_RULE
-include $(OUTDIR)/$(basename $(1))_*.d
$(OUTDIR)/$(basename $(1))_x86_32.o : $(1) $(TOP_MAKE) | $(dir $(OUTDIR)/$(basename $(1)))dir.stamp
	$(call LOG,CC,$$@,$(X86_32_CC) -o $$@ -c $$< $(POSIX_FLAGS) $(2) $(NACL_CFLAGS))

$(OUTDIR)/$(basename $(1))_x86_64.o : $(1) $(TOP_MAKE) | $(dir $(OUTDIR)/$(basename $(1)))dir.stamp
	$(call LOG,CC,$$@,$(X86_64_CC) -o $$@ -c $$< $(POSIX_FLAGS) $(2) $(NACL_CFLAGS))

$(OUTDIR)/$(basename $(1))_arm.o : $(1) $(TOP_MAKE) | $(dir $(OUTDIR)/$(basename $(1)))dir.stamp
	$(call LOG,CC,$$@,$(ARM_CC) -o $$@ -c $$< $(POSIX_FLAGS) $(2) $(NACL_CFLAGS))
endef

define CXX_COMPILER_RULE
-include $(OUTDIR)/$(basename $(1))_*.d
$(OUTDIR)/$(basename $(1))_x86_32.o : $(1) $(TOP_MAKE) | $(dir $(OUTDIR)/$(basename $(1)))dir.stamp
	$(call LOG,CXX,$$@,$(X86_32_CXX) -o $$@ -c $$< $(POSIX_FLAGS) $(2) $(NACL_CXXFLAGS))

$(OUTDIR)/$(basename $(1))_x86_64.o : $(1) $(TOP_MAKE) | $(dir $(OUTDIR)/$(basename $(1)))dir.stamp
	$(call LOG,CXX,$$@,$(X86_64_CXX) -o $$@ -c $$< $(POSIX_FLAGS) $(2) $(NACL_CXXFLAGS))

$(OUTDIR)/$(basename $(1))_arm.o : $(1) $(TOP_MAKE) | $(dir $(OUTDIR)/$(basename $(1)))dir.stamp
	$(call LOG,CXX,$$@,$(ARM_CXX) -o $$@ -c $$< $(POSIX_FLAGS) $(2) $(NACL_CXXFLAGS))
endef


#
# $1 = Source Name
# $2 = POSIX Compile Flags
# $3 = Include Directories
# $4 = VC Flags (unused)
#
define COMPILE_RULE
ifeq ('.c','$(suffix $(1))')
$(call C_COMPILER_RULE,$(1),$(2) $(foreach inc,$(INC_PATHS),-I$(inc)) $(3))
else
$(call CXX_COMPILER_RULE,$(1),$(2) $(foreach inc,$(INC_PATHS),-I$(inc)) $(3))
endif
endef


#
# SO Macro
#
# $1 = Target Name
# $2 = List of Sources
# $3 = List of LIBS
# $4 = List of DEPS
# $5 = 1 => Don't add to NMF.
#
#
GLIBC_REMAP:=
define SO_RULE
NMF_TARGETS+=$$(OUTDIR)/$(1)_x86_32.so
$(OUTDIR)/$(1)_x86_32.so : $(foreach src,$(2),$(OUTDIR)/$(basename $(src))_x86_32.o) $(4)
	$(call LOG,LINK,$$@,$(X86_32_LINK) -o $$@ $$(filter-out $(4),$$^) -shared -m32 $$(LD_X86_32) $$(LD_FLAGS) $(foreach lib,$(3),-l$(lib)))

NMF_TARGETS+=$(OUTDIR)/$(1)_x86_64.so
$(OUTDIR)/$(1)_x86_64.so : $(foreach src,$(2),$$(OUTDIR)/$(basename $(src))_x86_64.o) $(4)
	$(call LOG,LINK,$$@,$(X86_32_LINK) -o $$@ $$(filter-out $(4),$$^) -shared -m64 $(LD_X86_64) $$(LD_FLAGS) $(foreach lib,$(3),-l$(lib)))

ifneq (1,$(5))
GLIBC_SO_LIST+=$(OUTDIR)/$(1)_x86_32.so $(OUTDIR)/$(1)_x86_64.so
GLIBC_REMAP+=-n $(1)_x86_32.so,$(1).so
GLIBC_REMAP+=-n $(1)_x86_64.so,$(1).so
else
all: $(OUTDIR)/$(1)_x86_32.so $(OUTDIR)/$(1)_x86_64.so
endif
endef


#
# LIB Macro
#
# $1 = Target Name
# $2 = List of Sources
# $3 = POSIX Link Flags
# $4 = VC Link Flags (unused)
#
define LIB_RULE
$(STAMPDIR)/$(1).stamp : $(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_x86_32/$(CONFIG)/lib$(1).a
$(STAMPDIR)/$(1).stamp : $(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_x86_64/$(CONFIG)/lib$(1).a
ifneq ('glibc','$(TOOLCHAIN)')
$(STAMPDIR)/$(1).stamp : $(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_arm/$(CONFIG)/lib$(1).a
endif

$(STAMPDIR)/$(1).stamp :
	@echo "TOUCHED $$@" > $(STAMPDIR)/$(1).stamp

all: $(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_x86_32/$(CONFIG)/lib$(1).a
$(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_x86_32/$(CONFIG)/lib$(1).a : $(foreach src,$(2),$(OUTDIR)/$(basename $(src))_x86_32.o)
	$(MKDIR) -p $$(dir $$@)
	$(call LOG,LIB,$$@,$(X86_32_LIB) $$@ $$^)

all: $(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_x86_64/$(CONFIG)/lib$(1).a
$(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_x86_64/$(CONFIG)/lib$(1).a : $(foreach src,$(2),$(OUTDIR)/$(basename $(src))_x86_64.o)
	$(MKDIR) -p $$(dir $$@)
	$(call LOG,LIB,$$@,$(X86_64_LIB) $$@ $$^)

ifneq ('glibc','$(TOOLCHAIN)')
all: $(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_arm/$(CONFIG)/lib$(1).a
endif
$(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)_arm/$(CONFIG)/lib$(1).a : $(foreach src,$(2),$(OUTDIR)/$(basename $(src))_arm.o)
	$(MKDIR) -p $$(dir $$@)
	$(call LOG,LIB,$$@,$(ARM_LIB) $$@ $$^)
endef

#
# Specific Link Macro
#
# $1 = Target Name
# $2 = List of Sources
# $3 = List of LIBS
# $4 = List of DEPS
# $5 = POSIX Link Flags
# $6 = Library Paths
#
define LINKER_RULE
$(OUTDIR)/$(1)_x86_32.nexe : $(foreach src,$(2),$(OUTDIR)/$(basename $(src))_x86_32.o) $(foreach dep,$(4),$(STAMPDIR)/$(dep).stamp)
	$(call LOG,LINK,$$@,$(X86_32_LINK) -o $$@ $$(filter %.o,$$^) $(NACL_LDFLAGS) $(foreach path,$(6),-L$(path)/$(TOOLCHAIN)_x86_32/$(CONFIG)) $(foreach lib,$(3),-l$(lib)) $(5))

$(OUTDIR)/$(1)_x86_64.nexe : $(foreach src,$(2),$$(OUTDIR)/$(basename $(src))_x86_64.o) $(foreach dep,$(4),$(STAMPDIR)/$(dep).stamp)
	$(call LOG,LINK,$$@,$(X86_64_LINK) -o $$@ $$(filter %.o,$$^) $(NACL_LDFLAGS) $(foreach path,$(6),-L$(path)/$(TOOLCHAIN)_x86_64/$(CONFIG)) $(foreach lib,$(3),-l$(lib)) $(5))

$(OUTDIR)/$(1)_arm.nexe : $(foreach src,$(2),$(OUTDIR)/$(basename $(src))_arm.o) $(foreach dep,$(4),$(STAMPDIR)/$(dep).stamp)
	$(call LOG,LINK,$$@,$(ARM_LINK) -o $$@ $$(filter %.o,$$^) $(NACL_LDFLAGS) $(foreach path,$(6),-L$(path)/$(TOOLCHAIN)_arm/$(CONFIG)) $(foreach lib,$(3),-l$(lib)) $(5))
endef


#
# Generalized Link Macro
#
# $1 = Target Name
# $2 = List of Sources
# $3 = List of LIBS
# $4 = List of DEPS
# $5 = POSIX Linker Switches
# $6 = VC Linker Switches
#
define LINK_RULE
$(call LINKER_RULE,$(1),$(2),$(filter-out pthread,$(3)),$(4),$(5),$(LIB_PATHS))
endef


#
# Determine which architectures to build for.  The user can set NACL_ARCH or
# ARCHES in the environment to control this.
#
VALID_ARCHES:=x86_32 x86_64
ifeq (newlib,$(TOOLCHAIN))
VALID_ARCHES+=arm
endif

ifdef NACL_ARCH
ifeq (,$(findstring $(NACL_ARCH),$(VALID_ARCHES)))
$(error Invalid arch specified in NACL_ARCH: $(NACL_ARCH).  Valid values are: $(VALID_ARCHES))
endif
ARCHES=${NACL_ARCH}
else
ARCHES?=${VALID_ARCHES}
endif

#
# Generate NMF_TARGETS
#
NMF_ARCHES:=$(foreach arch,$(ARCHES),_$(arch).nexe)


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
all:$(OUTDIR)/$(1).nmf
$(OUTDIR)/$(1).nmf : $(foreach arch,$(NMF_ARCHES),$(OUTDIR)/$(1)$(arch)) $(GLIBC_SO_LIST)
	$(call LOG,CREATE_NMF,$$@,$(NMF) -o $$@ $$^ -D $(GLIBC_DUMP) $(GLIBC_PATHS) -s $(OUTDIR) $(2) $(GLIBC_REMAP))
endef


