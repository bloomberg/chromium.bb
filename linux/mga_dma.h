#ifndef MGA_DMA_H
#define MGA_DMA_H

#include "mga_drm_public.h"


/* Isn't this fun.  This has to be fixed asap by emitting primary
 * dma commands in the 'do_dma' ioctl.
 */
typedef struct {
	int dma_type;

   	unsigned int ContextState[MGA_CTX_SETUP_SIZE];
   	unsigned int ServerState[MGA_2D_SETUP_SIZE];
   	unsigned int TexState[2][MGA_TEX_SETUP_SIZE];
   	unsigned int WarpPipe;
   	unsigned int dirty;

   	unsigned int nbox;
   	xf86drmClipRectRec boxes[MGA_NR_SAREA_CLIPRECTS];
} drm_mga_buf_priv_t;


#define MGA_DMA_GENERAL 0
#define MGA_DMA_VERTEX  1
#define MGA_DMA_SETUP   2
#define MGA_DMA_ILOAD   3


#define DWGREG0 	0x1c00
#define DWGREG0_END 	0x1dff
#define DWGREG1		0x2c00
#define DWGREG1_END	0x2dff

#define ISREG0(r)	(r >= DWGREG0 && r <= DWGREG0_END)
#define ADRINDEX0(r)	(u8)((r - DWGREG0) >> 2)
#define ADRINDEX1(r)	(u8)(((r - DWGREG1) >> 2) | 0x80)
#define ADRINDEX(r)	(ISREG0(r) ? ADRINDEX0(r) : ADRINDEX1(r)) 


/* Macros for inserting commands into a secondary dma buffer.
 */

#define DMALOCALS	u8 tempIndex[4]; u32 *dma_ptr; \
			int outcount, num_dwords;

#define	DMAGETPTR(buf) do {			\
  dma_ptr = (u32 *)((u8 *)buf->address + buf->used);	\
  outcount = 0;					\
  num_dwords = buf->used / 4;			\
} while(0)

#define DMAADVANCE(buf)	do {			\
  buf->used = num_dwords * 4;			\
} while(0)

#define DMAOUTREG(reg, val) do {		\
  tempIndex[outcount]=ADRINDEX(reg);		\
  dma_ptr[++outcount] = val;			\
  if (outcount == 4) {				\
     outcount = 0;				\
     dma_ptr[0] = *(u32 *)tempIndex;		\
     dma_ptr+=5;				\
     num_dwords += 5;				\
  }						\
}while (0)



#define VERBO 0


/* Primary buffer versions of above -- pretty similar really.
 */
#define PRIMLOCALS	u8 tempIndex[4]; u32 *dma_ptr; u32 phys_head; \
			int outcount, num_dwords

#define PRIMRESET(dev_priv) do {				\
	dev_priv->prim_num_dwords = 0;				\
   	dev_priv->current_dma_ptr = dev_priv->prim_head;	\
} while (0)
	
#define	PRIMGETPTR(dev_priv) do {		\
	dma_ptr = dev_priv->current_dma_ptr;	\
	phys_head = dev_priv->prim_phys_head;	\
	num_dwords = dev_priv->prim_num_dwords;	\
	outcount = 0;				\
} while (0)

#define PRIMADVANCE(dev_priv)	do {		\
	dev_priv->prim_num_dwords = num_dwords;	\
	dev_priv->current_dma_ptr = dma_ptr;	\
} while (0)

#define PRIMOUTREG(reg, val) do {					\
	tempIndex[outcount]=ADRINDEX(reg);				\
	dma_ptr[1+outcount] = val;					\
	if( ++outcount == 4) {						\
		outcount = 0;						\
		dma_ptr[0] = *(u32 *)tempIndex;				\
		dma_ptr+=5;						\
		num_dwords += 5;					\
	}								\
	if (VERBO) 							\
		printk(KERN_INFO 					\
		       "OUT %x val %x dma_ptr %p nr_dwords %d\n",	\
		       outcount, ADRINDEX(reg), dma_ptr, 		\
		       num_dwords);   					\
}while (0)


#endif
