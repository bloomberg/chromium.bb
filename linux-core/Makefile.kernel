#
# Makefile for the drm device driver.  This driver provides support for
# the Direct Rendering Infrastructure (DRI) in XFree86 4.x.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

O_OBJS		:=
OX_OBJS		:=
M_OBJS		:=
MX_OBJS		:=

# Object file lists.

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

SUB_DIRS     := 
MOD_SUB_DIRS := $(SUB_DIRS)
ALL_SUB_DIRS := $(SUB_DIRS)

O_TARGET	:= drm.o
module-list     := gamma.o tdfx.o r128.o ffb.o mga.o i810.o
export-objs     := $(patsubst %.o,%_drv.o,$(module-list))

lib-objs   := init.o memory.o proc.o auth.o context.o drawable.o bufs.o
lib-objs   += lists.o lock.o ioctl.o fops.o vm.o dma.o ctxbitmap.o

ifneq ($(CONFIG_AGP),)
 lib-objs  += agpsupport.o
endif

gamma-objs := $(lib-objs) gamma_drv.o gamma_dma.o
tdfx-objs  := $(lib-objs) tdfx_drv.o                tdfx_context.o
r128-objs  := $(lib-objs) r128_drv.o  r128_dma.o    r128_context.o r128_bufs.o
ffb-objs   := $(lib-objs) ffb_drv.o                 ffb_context.o
mga-objs   := $(lib-objs) mga_drv.o   mga_dma.o     mga_context.o  mga_bufs.o \
	      mga_state.o
i810-objs  := $(lib-objs) i810_drv.o  i810_dma.o    i810_context.o i810_bufs.o

obj-$(CONFIG_DRM_GAMMA) += gamma.o $(gamma-objs)
obj-$(CONFIG_DRM_TDFX)  += tdfx.o  $(tdfx-objs)
obj-$(CONFIG_DRM_R128)  += r128.o  $(r128-objs)
obj-$(CONFIG_DRM_FFB)   += ffb.o   $(ffb-objs)

ifneq ($CONFIG_AGP),)
obj-$(CONFIG_DRM_MGA)   += mga.o   $(mga-objs)
obj-$(CONFIG_DRM_I810)  += i810.o  $(i810-objs)
endif

# Take module names out of obj-y and int-m

obj-y           := $(filter-out $(module-list), $(obj-y))
int-m           := $(filter-out $(module-list), $(obj-m))

# Translate to Rules.make lists.

O_OBJS          := $(filter-out $(export-objs), $(obj-y))
OX_OBJS         := $(filter     $(export-objs), $(obj-y))
M_OBJS          := $(sort $(filter     $(module-list), $(obj-m)))
MI_OBJS         := $(sort $(filter-out $(export-objs), $(int-m)))
MIX_OBJS        := $(sort $(filter     $(export-objs), $(int-m)))

include $(TOPDIR)/Rules.make

gamma.o: $(gamma-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(gamma-objs)

tdfx.o: $(tdfx-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(tdfx-objs)

mga.o: $(mga-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(mga-objs)

i810.o: $(i810-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(i810-objs)

r128.o: $(r128-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(r128-objs)

ffb.o: $(ffb-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(ffb-objs)
