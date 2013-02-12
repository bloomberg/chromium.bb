# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
#   http://www.gnu.org/software/make/manual/make.html
#
#


#
# Paths to Tools
#
PNACL_CC?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/newlib/bin/pnacl-clang -c
PNACL_CXX?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/newlib/bin/pnacl-clang++ -c
PNACL_LINK?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/newlib/bin/pnacl-clang++
PNACL_LIB?=$(TC_PATH)/$(OSNAME)_x86_$(TOOLCHAIN)/newlib/bin/pnacl-ar r


#
# Compile Macro
#
# $1 = Source Name
# $2 = Compile Flags
# $3 = Include Directories
#
define C_COMPILER_RULE
-include $(OUTDIR)/$(basename $(1))_pnacl.d
$(OUTDIR)/$(basename $(1))_pnacl.o : $(1) $(TOP_MAKE) | $(dir $(OUTDIR)/$(basename $(1)))dir.stamp
	$(call LOG,CC,$$@,$(PNACL_CC) -o $$@ -c $$< $(POSIX_FLAGS) $(2) $(NACL_CFLAGS))
endef

define CXX_COMPILER_RULE
-include $(OUTDIR)/$(basename $(1))_pnacl.d
$(OUTDIR)/$(basename $(1))_pnacl.o : $(1) $(TOP_MAKE) | $(dir $(OUTDIR)/$(basename $(1)))dir.stamp
	$(call LOG,CXX,$$@,$(PNACL_CXX) -o $$@ -c $$< $(POSIX_FLAGS) $(2) $(NACL_CFLAGS))
endef


# $1 = Source Name
# $2 = POSIX Compile Flags
# $3 = Include Directories
# $4 = VC Flags (unused)
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
#
#
define SO_RULE
$(error 'Shared libraries not supported by PNaCl')
endef


#
# LIB Macro
#
# $1 = Target Name
# $2 = List of Sources
# $3 = POSIX Link Flags
# $4 = VC Link Flags (unused)
define LIB_RULE
$(STAMPDIR)/$(1).stamp : $(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)/$(CONFIG)/lib$(1).a
	@echo "TOUCHED $$@" > $(STAMPDIR)/$(1).stamp

all: $(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)/$(CONFIG)/lib$(1).a
$(NACL_SDK_ROOT)/lib/$(TOOLCHAIN)/$(CONFIG)/lib$(1).a : $(foreach src,$(2),$(OUTDIR)/$(basename $(src))_pnacl.o)
	$(MKDIR) -p $$(dir $$@)
	$(call LOG,LIB,$$@,$(PNACL_LIB) $$@ $$^ $(3))
endef


#
# Specific Link Macro
#
# $1 = Target Name
# $2 = List of inputs
# $3 = List of libs
# $4 = List of deps
# $5 = List of lib dirs
# $6 = Other Linker Args
#
define LINKER_RULE
all: $(1)
$(1) : $(2) $(foreach dep,$(4),$(STAMPDIR)/$(dep).stamp)
	$(call LOG,LINK,$$@,$(PNACL_LINK) -o $(1) $(2) $(foreach path,$(5),-L$(path)/pnacl/$(CONFIG)) $(foreach lib,$(3),-l$(lib)) $(6))
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
$(call LINKER_RULE,$(OUTDIR)/$(1).pexe,$(foreach src,$(2),$(OUTDIR)/$(basename $(src))_pnacl.o),$(filter-out pthread,$(3)),$(4),$(LIB_PATHS),$(5))
endef



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

define NMF_RULE
all:$(OUTDIR)/$(1).nmf
$(OUTDIR)/$(1).nmf : $(OUTDIR)/$(1).pexe
	$(call LOG,CREATE_NMF,$$@,$(NMF) -o $$@ $$^ -s $(OUTDIR) $(2))
endef


