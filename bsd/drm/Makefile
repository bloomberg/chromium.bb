# $FreeBSD$

KMOD	= drm
SRCS	= init.c memory.c auth.c context.c drawable.c bufs.c \
	  lists.c lock.c ioctl.c fops.c vm.c dma.c sysctl.c  \
	  agpsupport.c
SRCS   += device_if.h bus_if.h pci_if.h
CFLAGS += ${DEBUG_FLAGS} -I.. # -DDRM_AGP
KERN    = /usr/src/sys

@:
	ln -sf /sys @

machine:
	ln -sf /sys/i386/include machine

.include "/usr/src/sys/conf/kmod.mk"
