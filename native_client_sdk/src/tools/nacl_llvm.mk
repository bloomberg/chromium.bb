# Copyright (c) 2012 The Chromium Authors.   All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
#   http://www.gnu.org/software/make/manual/make.html
#

#
# Paths to Tools
#
PNACL_BIN = $(TC_PATH)/$(OSNAME)_$(TOOLCHAIN)/bin
PNACL_CC ?= $(PNACL_BIN)/pnacl-clang -c
PNACL_CXX ?= $(PNACL_BIN)/pnacl-clang++ -c
PNACL_LINK ?= $(PNACL_BIN)/pnacl-clang++
PNACL_LIB ?= $(PNACL_BIN)/pnacl-ar
PNACL_STRIP ?= $(PNACL_BIN)/pnacl-strip
PNACL_FINALIZE ?= $(PNACL_BIN)/pnacl-finalize
PNACL_TRANSLATE ?= $(PNACL_BIN)/pnacl-translate

#
# Compile Macro
#
# $1 = Source Name
# $2 = Compile Flags
# $3 = Include Directories
#
define C_COMPILER_RULE
-include $(call SRC_TO_DEP,$(1))
$(call SRC_TO_OBJ,$(1)): $(1) $(TOP_MAKE) | $(dir $(call SRC_TO_OBJ,$(1)))dir.stamp
	$(call LOG,CC  ,$$@,$(PNACL_CC) -o $$@ -c $$< $(POSIX_FLAGS) $(2) $(NACL_CFLAGS))
	@$(FIXDEPS) $(call SRC_TO_DEP_PRE_FIXUP,$(1))
endef

define CXX_COMPILER_RULE
-include $(call SRC_TO_DEP,$(1))
$(call SRC_TO_OBJ,$(1)): $(1) $(TOP_MAKE) | $(dir $(call SRC_TO_OBJ,$(1)))dir.stamp
	$(call LOG,CXX ,$$@,$(PNACL_CXX) -o $$@ -c $$< $(POSIX_FLAGS) $(2) $(NACL_CFLAGS))
	@$(FIXDEPS) $(call SRC_TO_DEP_PRE_FIXUP,$(1))
endef


# $1 = Source Name
# $2 = POSIX Compile Flags
# $3 = Include Directories
# $4 = VC Flags (unused)
define COMPILE_RULE
ifeq ($(suffix $(1)),.c)
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
$(STAMPDIR)/$(1).stamp: $(LIBDIR)/$(TOOLCHAIN)/$(CONFIG)/lib$(1).a
	@echo "TOUCHED $$@" > $(STAMPDIR)/$(1).stamp

all: $(LIBDIR)/$(TOOLCHAIN)/$(CONFIG)/lib$(1).a
$(LIBDIR)/$(TOOLCHAIN)/$(CONFIG)/lib$(1).a: $(foreach src,$(2),$(call SRC_TO_OBJ,$(src)))
	$(MKDIR) -p $$(dir $$@)
	$(RM) -f $$@
	$(call LOG,LIB,$$@,$(PNACL_LIB) -cr $$@ $$^ $(3))
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
# For debugging, we translate the pre-finalized .bc file.
#
define LINKER_RULE
all: $(1).pexe 
$(1)_x86_32.nexe : $(1).bc
	$(call LOG,TRANSLATE,$$@,$(PNACL_TRANSLATE) --allow-llvm-bitcode-input -arch x86-32 $$^ -o $$@)

$(1)_x86_64.nexe : $(1).bc
	$(call LOG,TRANSLATE,$$@,$(PNACL_TRANSLATE) --allow-llvm-bitcode-input -arch x86-64 $$^ -o $$@)

$(1)_arm.nexe : $(1).bc
	$(call LOG,TRANSLATE,$$@,$(PNACL_TRANSLATE) --allow-llvm-bitcode-input -arch arm $$^ -o $$@)

$(1).pexe: $(1).bc
	$(call LOG,FINALIZE,$$@,$(PNACL_FINALIZE) -o $$@ $$^)

$(1).bc: $(2) $(foreach dep,$(4),$(STAMPDIR)/$(dep).stamp)
	$(call LOG,LINK,$$@,$(PNACL_LINK) -o $$@ $(2) $(PNACL_LDFLAGS) $(foreach path,$(5),-L$(path)/pnacl/$(CONFIG)) $(foreach lib,$(3),-l$(lib)) $(6))
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
# NOTE:  For Debug builds we translate the .bc file to a .nexe instead of
# using the finalizing to a .pexe.  This enables debugging.
#
define LINK_RULE
$(call LINKER_RULE,$(OUTDIR)/$(1),$(foreach src,$(2),$(call SRC_TO_OBJ,$(src))),$(filter-out pthread,$(3)),$(4),$(LIB_PATHS),$(5))
endef


#
# Strip Macro
#
# NOTE: pnacl-strip does not currently support stripping finalized pexes (in a
# sense, they are already stripped). So we just copy the file instead.
#
# See https://code.google.com/p/nativeclient/issues/detail?id=3534
#
# $1 = Target Name
# $2 = Input Name
#
define STRIP_RULE
all: $(OUTDIR)/$(1).pexe
$(OUTDIR)/$(1).pexe: $(OUTDIR)/$(2).pexe
	$(CP) $$^ $$@
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

ifeq ($(CONFIG),Debug)
EXECUTABLES=$(OUTDIR)/$(1)_x86_32.nexe $(OUTDIR)/$(1)_x86_64.nexe $(OUTDIR)/$(1)_arm.nexe
else
EXECUTABLES=$(OUTDIR)/$(1).pexe
endif

define NMF_RULE
all: $(OUTDIR)/$(1).nmf
$(OUTDIR)/$(1).nmf: $(EXECUTABLES)
	$(call LOG,CREATE_NMF,$$@,$(NMF) -o $$@ $$^ -s $(OUTDIR) $(2))
endef

#
# HTML file generation
#
CREATE_HTML := python $(NACL_SDK_ROOT)/tools/create_html.py

define HTML_RULE
all: $(OUTDIR)/$(1).html
$(OUTDIR)/$(1).html: $(EXECUTABLES)
	$(call LOG,CREATE_HTML,$$@,$(CREATE_HTML) -o $$@ $$^)
endef


#
# Determine which executable to pass into the debugger.  For pnacl, this is
# the .bc -> .nexe translated app.
#
GDB_DEBUG_TARGET = $(abspath $(OUTDIR))/$(TARGET)_$(SYSARCH).nexe
