
/****************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.			
 *																			*
 * All Rights Reserved.														*
 *																			*
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the	
 * "Software"), to deal in the Software without restriction, including	
 * without limitation on the rights to use, copy, modify, merge,	
 * publish, distribute, sublicense, and/or sell copies of the Software,	
 * and to permit persons to whom the Software is furnished to do so,	
 * subject to the following conditions:					
 *																			*
 * The above copyright notice and this permission notice (including the	
 * next paragraph) shall be included in all copies or substantial	
 * portions of the Software.						
 *																			*
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,	
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF	
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND		
 * NON-INFRINGEMENT.  IN NO EVENT SHALL XGI AND/OR			
 * ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,		
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,		
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER			
 * DEALINGS IN THE SOFTWARE.												
 ***************************************************************************/

#ifndef _XGI_TYPES_H_
#define _XGI_TYPES_H_

/****************************************************************************
 *                                 Typedefs                                 *
 ***************************************************************************/

typedef unsigned char       V8;  /* "void": enumerated or multiple fields   */
typedef unsigned short      V16; /* "void": enumerated or multiple fields   */
typedef unsigned char       U8;  /* 0 to 255                                */
typedef unsigned short      U16; /* 0 to 65535                              */
typedef signed char         S8;  /* -128 to 127                             */
typedef signed short        S16; /* -32768 to 32767                         */
typedef float               F32; /* IEEE Single Precision (S1E8M23)         */
typedef double              F64; /* IEEE Double Precision (S1E11M52)        */
typedef unsigned long       BOOL;
/*
 * mainly for 64-bit linux, where long is 64 bits
 * and win9x, where int is 16 bit.
 */
#if defined(vxworks)
typedef unsigned int       V32; /* "void": enumerated or multiple fields   */
typedef unsigned int       U32; /* 0 to 4294967295                         */
typedef signed int         S32; /* -2147483648 to 2147483647               */
#else
typedef unsigned long      V32; /* "void": enumerated or multiple fields   */
typedef unsigned long      U32; /* 0 to 4294967295                         */
typedef signed long        S32; /* -2147483648 to 2147483647               */
#endif

#ifndef TRUE
#define TRUE    1UL
#endif

#ifndef FALSE
#define FALSE   0UL
#endif

#endif

