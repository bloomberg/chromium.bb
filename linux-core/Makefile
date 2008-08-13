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
O := $(shell if [ -e /lib/modules/$(RUNNING_REL)/build ]; then \
		 echo /lib/modules/$(RUNNING_REL)/build; \
		 else echo ""; fi)
#O := $(LINUXDIR)
endif

ifdef ARCH
MACHINE := $(ARCH)
else
MACHINE := $(shell uname -m)
endif

# Modules for all architectures
MODULE_LIST := drm.o tdfx.o r128.o radeon.o mga.o sis.o savage.o via.o \
               mach64.o nv.o nouveau.o xgi.o radeon_ms.o

# Modules only for ix86 architectures
ifneq (,$(findstring 86,$(MACHINE)))
ARCHX86 := 1
MODULE_LIST += i810.o i915.o
endif

ifneq (,$(findstring sparc64,$(MACHINE)))
ARCHSPARC64 := 1
#MODULE_LIST += ffb.o
endif

DRM_MODULES ?= $(MODULE_LIST)

# These definitions are for handling dependencies in the out of kernel build.

DRMHEADERS =    drmP.h drm_compat.h drm_os_linux.h drm.h drm_sarea.h
COREHEADERS =   drm_core.h drm_sman.h drm_hashtab.h 

TDFXHEADERS =   tdfx_drv.h $(DRMHEADERS)
R128HEADERS =   r128_drv.h r128_drm.h $(DRMHEADERS)
RADEONHEADERS = radeon_drv.h radeon_drm.h r300_reg.h $(DRMHEADERS)
MGAHEADERS =    mga_drv.h mga_drm.h mga_ucode.h $(DRMHEADERS)
I810HEADERS =   i810_drv.h i810_drm.h $(DRMHEADERS)
I915HEADERS =   i915_drv.h i915_drm.h $(DRMHEADERS)
SISHEADERS=     sis_drv.h sis_drm.h drm_hashtab.h drm_sman.h $(DRMHEADERS)
SAVAGEHEADERS=  savage_drv.h savage_drm.h $(DRMHEADERS)
VIAHEADERS =	via_drm.h via_drv.h via_3d_reg.h via_verifier.h $(DRMHEADERS)
MACH64HEADERS = mach64_drv.h mach64_drm.h $(DRMHEADERS)
NVHEADERS =     nv_drv.h $(DRMHEADERS)
FFBHEADERS = 	ffb_drv.h $(DRMHEADERS)
NOUVEAUHEADERS = nouveau_drv.h nouveau_drm.h nouveau_reg.h nouveau_display.h $(DRMHEADERS)
XGIHEADERS = xgi_cmdlist.h xgi_drv.h xgi_misc.h xgi_regs.h $(DRMHEADERS)
RADEONMSHEADERS = radeon_ms_driver.h $(DRMHEADERS) 

PROGS = dristat drmstat

CLEANFILES = *.o *.ko $(PROGS) .depend .*.flags .*.d .*.cmd *.mod.c drm_pciids.h .tmp_versions

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
GETCONFIG := MAKEFILES=$(shell /bin/pwd)/.config
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
	+make -C $(LINUXDIR) $(GETCONFIG) SUBDIRS=`/bin/pwd` DRMSRCDIR=`/bin/pwd` modules

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

ifeq ($(shell if [ -d $(SHAREDDIR) ]; then echo y; fi),y)
includes::  drm_pciids.h

drm_pciids.h: $(SHAREDDIR)/drm_pciids.txt
	sh ../scripts/create_linux_pci_lists.sh < $(SHAREDDIR)/drm_pciids.txt
endif

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

install:
	make -C $(LINUXDIR) $(GETCONFIG) SUBDIRS=`/bin/pwd` DRMSRCDIR=`/bin/pwd` modules_install

else

# Check for kernel versions that we don't support.

BELOW26 := $(shell if [ $(VERSION) -lt 2 -o $(PATCHLEVEL) -lt 6 ]; then \
		echo y; fi)

ifeq ($(BELOW26),y)
$(error Only 2.6.x and later kernels are supported \
	($(VERSION).$(PATCHLEVEL).$(SUBLEVEL)))
endif

ifdef ARCHX86
ifndef CONFIG_X86_CMPXCHG
$(error CONFIG_X86_CMPXCHG needs to be enabled in the kernel)
endif
endif

# This needs to go before all other include paths.
CC += -I$(DRMSRCDIR)

# Check for PAGE_AGP definition
PAGE_AGP := $(shell cat $(LINUXDIR)/include/asm/agp.h 2>/dev/null | \
		grep -c PAGE_AGP)

ifneq ($(PAGE_AGP),0)
EXTRA_CFLAGS += -DHAVE_PAGE_AGP
endif

ifeq ($(OS_HAS_GEM), 1)
EXTRA_CFLAGS += -DOS_HAS_GEM=1
endif

# Start with all modules turned off.
CONFIG_DRM_GAMMA := n
CONFIG_DRM_TDFX := n
CONFIG_DRM_MGA := n
CONFIG_DRM_I810 := n
CONFIG_DRM_R128 := n
CONFIG_DRM_RADEON := n
CONFIG_DRM_I915 := n
CONFIG_DRM_SIS := n
CONFIG_DRM_FFB := n
CONFIG_DRM_SAVAGE := n
CONFIG_DRM_VIA := n
CONFIG_DRM_MACH64 := n
CONFIG_DRM_NV := n
CONFIG_DRM_NOUVEAU := n
CONFIG_DRM_XGI := n
CONFIG_DRM_RADEON_MS := n

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
ifneq (,$(findstring nouveau,$(DRM_MODULES)))
CONFIG_DRM_NOUVEAU := m
endif
ifneq (,$(findstring xgi,$(DRM_MODULES)))
CONFIG_DRM_XGI := m
endif
ifneq (,$(findstring radeon_ms,$(DRM_MODULES)))
#CONFIG_DRM_RADEON_MS := m
endif

# These require AGP support

ifneq (,$(findstring i810,$(DRM_MODULES)))
CONFIG_DRM_I810 := m
endif
ifneq (,$(findstring i915,$(DRM_MODULES)))
ifeq ($(OS_HAS_GEM), 1)
CONFIG_DRM_I915 := m
endif
endif

GIT_REVISION := $(shell cd "$(DRMSRCDIR)" && git-describe --abbrev=17)
ifneq ($(GIT_REVISION),)
EXTRA_CFLAGS+=-D"GIT_REVISION=\"$(GIT_REVISION)\""
endif

include $(DRMSRCDIR)/Makefile.kernel

# Depencencies
$(drm-objs):	$(DRMHEADERS) $(COREHEADERS)
$(tdfx-objs):	$(TDFXHEADERS)
$(r128-objs):	$(R128HEADERS)
$(mga-objs):	$(MGAHEADERS)
$(i810-objs):	$(I810HEADERS)
$(i915-objs):	$(I915HEADERS)
$(radeon-objs):	$(RADEONHEADERS)
$(sis-objs):	$(SISHEADERS)
$(ffb-objs):	$(FFBHEADERS)
$(savage-objs): $(SAVAGEHEADERS)
$(via-objs):	$(VIAHEADERS)
$(mach64-objs): $(MACH64HEADERS)
$(nv-objs):     $(NVHEADERS)
$(nouveau-objs): $(NOUVEAUHEADERS)
$(xgi-objs):    $(XGIHEADERS)

endif

