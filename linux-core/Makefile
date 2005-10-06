# Makefile -- For the Direct Rendering Manager module (drm)
#
# Based on David Woodhouse's mtd build.
#
# Modified to handle the DRM requirements and builds on a wider range of
# platforms in a flexible way by David Dawes.  It's not clear, however,
# that this approach is simpler than the old one.
#
# The purpose of this Makefile is to handle setting up everything
# needed for an out-of-kernel source build.  Makefile.kernel contains
# everything required for in-kernel source builds.  It is included into
# this file, so none of that should be duplicated here.
#
# $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/Makefile.linux,v 1.40 2003/08/17 17:12:25 dawes Exp $
#

#
# By default, the build is done against the running linux kernel source.
# To build against a different kernel source tree, set LINUXDIR:
#
#    make LINUXDIR=/path/to/kernel/source

#
# To build only some modules, either set DRM_MODULES to the list of modules,
# or specify the modules as targets:
#
#    make r128.o radeon.o
#
# or:
#
#    make DRM_MODULES="r128 radeon"
#

SHELL=/bin/sh

.SUFFIXES:

ifndef LINUXDIR
RUNNING_REL := $(shell uname -r)

LINUXDIR := $(shell if [ -e /lib/modules/$(RUNNING_REL)/source ]; then \
		 echo /lib/modules/$(RUNNING_REL)/source; \
		 else echo /lib/modules/$(RUNNING_REL)/build; fi)
endif

ifndef O
O := $(LINUXDIR)
endif

MACHINE := $(shell uname -m)

# Modules for all architectures
MODULE_LIST := drm.o tdfx.o r128.o radeon.o mga.o sis.o savage.o via.o \
               mach64.o nv.o

# Modules only for ix86 architectures
ifneq (,$(findstring 86,$(MACHINE)))
ARCHX86 := 1
MODULE_LIST += i830.o i810.o i915.o
endif

ifneq (,$(findstring sparc64,$(MACHINE)))
ARCHSPARC64 := 1
MODULE_LIST += ffb.o
endif

DRM_MODULES ?= $(MODULE_LIST)

# These definitions are for handling dependencies in the out of kernel build.

DRMSHARED =     drm.h drm_sarea.h
DRMHEADERS =    drmP.h drm_compat.h drm_os_linux.h $(DRMSHARED)
COREHEADERS =   drm_core.h 

TDFXHEADERS =   tdfx_drv.h $(DRMHEADERS)
TDFXSHARED =    tdfx_drv.h
R128HEADERS =   r128_drv.h r128_drm.h $(DRMHEADERS)
R128SHARED =    r128_drv.h r128_drm.h r128_cce.c r128_state.c r128_irq.c
RADEONHEADERS = radeon_drv.h radeon_drm.h r300_reg.h $(DRMHEADERS)
RADEONSHARED =  radeon_drv.h radeon_drm.h radeon_cp.c radeon_irq.c \
                radeon_mem.c radeon_state.c r300_cmdbuf.c r300_reg.h
MGAHEADERS =    mga_drv.h mga_drm.h mga_ucode.h $(DRMHEADERS)
MGASHARED =     mga_dma.c mga_drm.h mga_drv.h mga_irq.c mga_state.c \
                mga_ucode.h mga_warp.c
I810HEADERS =   i810_drv.h i810_drm.h $(DRMHEADERS)
I830HEADERS =   i830_drv.h i830_drm.h $(DRMHEADERS)
I915HEADERS =   i915_drv.h i915_drm.h $(DRMHEADERS)
I915SHARED  =   i915_drv.h i915_drm.h i915_irq.c i915_mem.c i915_dma.c
SISHEADERS=     sis_drv.h sis_drm.h $(DRMHEADERS)
SISSHARED=      sis_drv.h sis_drm.h sis_ds.c sis_ds.h sis_mm.c
SAVAGEHEADERS=  savage_drv.h savage_drm.h $(DRMHEADERS)
SAVAGESHARED=	savage_drv.h savage_drm.h savage_bci.c savage_state.c
VIAHEADERS =	via_drm.h via_drv.h via_mm.h via_ds.h \
		via_3d_reg.h via_verifier.h $(DRMHEADERS)
VIASHARED	= via_drm.h via_drv.h via_mm.h via_ds.h \
		via_3d_reg.h via_drv.c via_ds.c via_irq.c via_map.c \
		via_mm.c via_dma.c via_verifier.c via_verifier.h via_video.c
MACH64HEADERS = mach64_drv.h mach64_drm.h $(DRMHEADERS)
MACH64SHARED = 	mach64_drv.h mach64_drm.h mach64_dma.c \
		mach64_irq.c mach64_state.c
NVHEADERS =     nv_drv.h $(DRMHEADERS)
NVSHARED  =     nv_drv.h
FFBHEADERS = 	ffb_drv.h $(DRMHEADERS)

SHAREDSRC = $(DRMSHARED) $(MGASHARED) $(R128SHARED) $(RADEONSHARED) \
	$(SISSHARED) $(TDFXSHARED) $(VIASHARED) $(MACH64SHARED) \
	$(I915SHARED) $(SAVAGESHARED) $(NVSHARED)

PROGS = dristat drmstat

CLEANFILES = *.o *.ko $(PROGS) .depend .*.flags .*.d .*.cmd *.mod.c linux drm_pciids.h .tmp_versions

# VERSION is not defined from the initial invocation.  It is defined when
# this Makefile is invoked from the kernel's root Makefile.

ifndef VERSION

ifdef RUNNING_REL

# SuSE has the version.h and autoconf.h headers for the current kernel
# in /boot as /boot/vmlinuz.version.h and /boot/vmlinuz.autoconf.h.
# Check these first to see if they match the running kernel.

BOOTVERSION_PREFIX = /boot/vmlinuz.

V := $(shell if [ -f $(BOOTVERSION_PREFIX)version.h ]; then \
	grep UTS_RELEASE $(BOOTVERSION_PREFIX)version.h | \
	cut -d' ' -f3; fi)

ifeq ($(V),"$(RUNNING_REL)")
HEADERFROMBOOT := 1
GETCONFIG := MAKEFILES=$(shell pwd)/.config
HAVECONFIG := y
endif

# On Red Hat we need to check if there is a .config file in the kernel
# source directory.  If there isn't, we need to check if there's a
# matching file in the configs subdirectory.

ifneq ($(HAVECONFIG),y)
HAVECONFIG := $(shell if [ -e $(LINUXDIR)/.config ]; then echo y; fi)
endif

ifneq ($(HAVECONFIG),y)
REL_BASE := $(shell echo $(RUNNING_REL) | sed 's/-.*//')
REL_TYPE := $(shell echo $(RUNNING_REL) | sed 's/[0-9.-]//g')
ifeq ($(REL_TYPE),)
RHCONFIG := configs/kernel-$(REL_BASE)-$(MACHINE).config
else
RHCONFIG := configs/kernel-$(REL_BASE)-$(MACHINE)-$(REL_TYPE).config
endif
HAVECONFIG := $(shell if [ -e $(LINUXDIR)/$(RHCONFIG) ]; then echo y; fi)
ifneq ($(HAVECONFIG),y)
RHCONFIG :=
endif
endif

ifneq ($(HAVECONFIG),y)
ifneq ($(0),$(LINUXDIR))
GETCONFIG += O=$(O)
endif
HAVECONFIG := $(shell if [ -e $(O)/.config ]; then echo y; fi)
endif

ifneq ($(HAVECONFIG),y)
$(error Cannot find a kernel config file)
endif

endif

CLEANCONFIG := $(shell if cmp -s $(LINUXDIR)/.config .config; then echo y; fi)
ifeq ($(CLEANCONFIG),y)
CLEANFILES += $(LINUXDIR)/.config .config $(LINUXDIR)/tmp_include_depends
endif

all: modules

modules: includes
	make -C $(LINUXDIR) $(GETCONFIG) SUBDIRS=`pwd` DRMSRCDIR=`pwd` modules

ifeq ($(HEADERFROMBOOT),1)

BOOTHEADERS = version.h autoconf.h
BOOTCONFIG = .config

CLEANFILES += $(BOOTHEADERS) $(BOOTCONFIG)

includes:: $(BOOTHEADERS) $(BOOTCONFIG)

version.h: $(BOOTVERSION_PREFIX)version.h
	rm -f $@
	ln -s $< $@

autoconf.h: $(BOOTVERSION_PREFIX)autoconf.h
	rm -f $@
	ln -s $< $@

.config: $(BOOTVERSION_PREFIX)config
	rm -f $@
	ln -s $< $@
endif

# This prepares an unused Red Hat kernel tree for the build.
ifneq ($(RHCONFIG),)
includes:: $(LINUXDIR)/.config $(LINUXDIR)/tmp_include_depends .config

$(LINUXDIR)/.config: $(LINUXDIR)/$(RHCONFIG)
	rm -f $@
	ln -s $< $@

.config: $(LINUXDIR)/$(RHCONFIG)
	rm -f $@
	ln -s $< $@

$(LINUXDIR)/tmp_include_depends:
	echo all: > $@
endif

# Make sure that the shared source files are linked into this directory.


SHAREDDIR := ../shared-core

HASSHARED := $(shell if [ -d $(SHAREDDIR) ]; then echo y; fi)

ifeq ($(HASSHARED),y)
includes:: $(SHAREDSRC) drm_pciids.h

drm_pciids.h: $(SHAREDDIR)/drm_pciids.txt
	sh ../scripts/create_linux_pci_lists.sh < $(SHAREDDIR)/drm_pciids.txt

$(SHAREDSRC):
	@if [ -r $(SHAREDDIR)/$@ ]; then \
		(rm -f $@; set -x; ln -s $(SHAREDDIR)/$@ $@); fi

CLEANFILES += $(SHAREDSRC)
endif

includes:: linux

linux:
	rm -f linux
	ln -s . linux

clean cleandir:
	rm -rf $(CLEANFILES)

$(MODULE_LIST)::
	make DRM_MODULES=$@ modules

# Build test utilities

PRGCFLAGS = $(CFLAGS) -g -ansi -pedantic -DPOSIX_C_SOURCE=199309L \
	    -D_POSIX_SOURCE -D_XOPEN_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE \
	    -I. -I../../..

DRMSTATLIBS = -L../../.. -L.. -ldrm -lxf86_os \
	      -L../../../../dummylib -ldummy -lm

programs: $(PROGS)

dristat: dristat.c
	$(CC) $(PRGCFLAGS) $< -o $@

drmstat: drmstat.c
	$(CC) $(PRGCFLAGS) $< -o $@ $(DRMSTATLIBS)

else

# Check for kernel versions that we don't support.

BELOW24 := $(shell if [ $(VERSION) -lt 2 -o $(PATCHLEVEL) -lt 4 ]; then \
		echo y; fi)

ifeq ($(BELOW24),y)
$(error Only 2.4.x and later kernels are supported \
	($(VERSION).$(PATCHLEVEL).$(SUBLEVEL)))
endif

ifdef ARCHX86
ifndef CONFIG_X86_CMPXCHG
$(error CONFIG_X86_CMPXCHG needs to be enabled in the kernel)
endif
endif

# This needs to go before all other include paths.
CC += -I$(DRMSRCDIR)

# Check for Red Hat's 4-argument do_munmap().
DOMUNMAP := $(shell grep do_munmap $(LINUXDIR)/include/linux/mm.h | \
                grep -c acct)

ifneq ($(DOMUNMAP),0)
EXTRA_CFLAGS += -DDO_MUNMAP_4_ARGS
endif

# Check for 5-argument remap_page_range() in RH9 kernel, and 2.5.x kernels
RPR := $(shell grep remap_page_range $(LINUXDIR)/include/linux/mm.h | \
		grep -c vma)

ifneq ($(RPR),0)
EXTRA_CFLAGS += -DREMAP_PAGE_RANGE_5_ARGS
endif

# Check for 4-argument vmap() in some 2.5.x and 2.4.x kernels
VMAP := $(shell grep -A1 'vmap.*count,$$' $(LINUXDIR)/include/linux/vmalloc.h | \
		grep -c prot)

ifneq ($(VMAP),0)
EXTRA_CFLAGS += -DVMAP_4_ARGS
endif

# Check for PAGE_AGP definition
PAGE_AGP := $(shell cat $(LINUXDIR)/include/asm/agp.h 2>/dev/null | \
		grep -c PAGE_AGP)

ifneq ($(PAGE_AGP),0)
EXTRA_CFLAGS += -DHAVE_PAGE_AGP
endif


# Start with all modules turned off.
CONFIG_DRM_GAMMA := n
CONFIG_DRM_TDFX := n
CONFIG_DRM_MGA := n
CONFIG_DRM_I810 := n
CONFIG_DRM_R128 := n
CONFIG_DRM_RADEON := n
CONFIG_DRM_I830 := n
CONFIG_DRM_I915 := n
CONFIG_DRM_SIS := n
CONFIG_DRM_FFB := n
CONFIG_DRM_SAVAGE := n
CONFIG_DRM_VIA := n
CONFIG_DRM_MACH64 := n
CONFIG_DRM_NV := n

# Enable module builds for the modules requested/supported.

ifneq (,$(findstring tdfx,$(DRM_MODULES)))
CONFIG_DRM_TDFX := m
endif
ifneq (,$(findstring r128,$(DRM_MODULES)))
CONFIG_DRM_R128 := m
endif
ifneq (,$(findstring radeon,$(DRM_MODULES)))
CONFIG_DRM_RADEON := m
endif
ifneq (,$(findstring sis,$(DRM_MODULES)))
CONFIG_DRM_SIS := m
endif
ifneq (,$(findstring via,$(DRM_MODULES)))
CONFIG_DRM_VIA := m
endif
ifneq (,$(findstring mach64,$(DRM_MODULES)))
CONFIG_DRM_MACH64 := m
endif
ifneq (,$(findstring ffb,$(DRM_MODULES)))
CONFIG_DRM_FFB := m
endif
ifneq (,$(findstring savage,$(DRM_MODULES)))
CONFIG_DRM_SAVAGE := m
endif
ifneq (,$(findstring mga,$(DRM_MODULES)))
CONFIG_DRM_MGA := m
endif
ifneq (,$(findstring nv,$(DRM_MODULES)))
CONFIG_DRM_NV := m
endif

# These require AGP support

ifdef CONFIG_AGP
ifneq (,$(findstring i810,$(DRM_MODULES)))
CONFIG_DRM_I810 := m
endif
ifneq (,$(findstring i830,$(DRM_MODULES)))
CONFIG_DRM_I830 := m
endif
ifneq (,$(findstring i915,$(DRM_MODULES)))
CONFIG_DRM_I915 := m
endif
endif

include $(DRMSRCDIR)/Makefile.kernel

# Depencencies
$(drm-objs):	$(DRMHEADERS) $(COREHEADERS)
$(tdfx-objs):	$(TDFXHEADERS)
$(r128-objs):	$(R128HEADERS)
$(mga-objs):	$(MGAHEADERS)
$(i810-objs):	$(I810HEADERS)
$(i830-objs):	$(I830HEADERS)
$(i915-objs):	$(I915HEADERS)
$(radeon-objs):	$(RADEONHEADERS)
$(sis-objs):	$(SISHEADERS)
$(ffb-objs):	$(FFBHEADERS)
$(savage-objs): $(SAVAGEHEADERS)
$(via-objs):	$(VIAHEADERS)
$(mach64-objs): $(MACH64HEADERS)
$(nv-objs):     $(NVHEADERS)

endif

