# $FreeBSD$

KMOD	= drm
SRCS	= init.c memory.c auth.c context.c drawable.c bufs.c \
	  lists.c lock.c ioctl.c fops.c vm.c dma.c sysctl.c  \
	  agpsupport.c ctxbitmap.c
SRCS   += device_if.h bus_if.h pci_if.h
CFLAGS += ${DEBUG_FLAGS} -I..

@:
	ln -sf /sys @

machine:
	ln -sf /sys/i386/include machine

.include <bsd.kmod.mk>
