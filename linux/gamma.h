#ifndef __GAMMA_H__
#define __GAMMA_H__

/* This remains constant for all DRM template files.
 */
#define DRM(x) gamma_##x

#define __HAVE_MTRR		1
#define __HAVE_PCI_DMA		1
#define __HAVE_CTX_BITMAP	0
#define __HAVE_MULTIPLE_DMA_QUEUES	1
#define __HAVE_DMA_FLUSH		1
#define __HAVE_DMA_QUEUE		0
#define __HAVE_DMA_SCHEDULE		1
#define __HAVE_DMA_WAITQUEUE		1
#define __HAVE_DMA_WAITLIST	1
#define __HAVE_DMA_FREELIST	1
#define __HAVE_DMA		1
#define __HAVE_OLD_DMA		1
#define __HAVE_DMA_IRQ		1

#endif
