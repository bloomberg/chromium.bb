# $FreeBSD$

KMOD	= mga
SRCS    = mga_drv.c mga_context.c mga_state.c mga_bufs.c mga_dma.c
SRCS   += device_if.h bus_if.h pci_if.h
CFLAGS += ${DEBUG_FLAGS} -I..
KERN    = /usr/src/sys
KMODDEPS = drm

@:
	ln -sf /sys @

machine:
	ln -sf /sys/i386/include machine

.include <bsd.kmod.mk>
