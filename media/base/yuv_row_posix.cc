// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/yuv_row.h"

#ifdef _DEBUG
#include "base/logging.h"
#else
#define DCHECK(a)
#endif

extern "C" {

#if USE_SSE2 && defined(ARCH_CPU_X86_64)

// AMD64 ABI uses register paremters.
void FastConvertYUVToRGB32Row(const uint8* y_buf,  // rdi
                              const uint8* u_buf,  // rsi
                              const uint8* v_buf,  // rdx
                              uint8* rgb_buf,      // rcx
                              int width) {         // r8
  asm(
  "jmp    convertend\n"
"convertloop:"
  "movzb  (%1),%%r10\n"
  "add    $0x1,%1\n"
  "movzb  (%2),%%r11\n"
  "add    $0x1,%2\n"
  "movq   2048(%5,%%r10,8),%%xmm0\n"
  "movzb  (%0),%%r10\n"
  "movq   4096(%5,%%r11,8),%%xmm1\n"
  "movzb  0x1(%0),%%r11\n"
  "paddsw %%xmm1,%%xmm0\n"
  "movq   (%5,%%r10,8),%%xmm2\n"
  "add    $0x2,%0\n"
  "movq   (%5,%%r11,8),%%xmm3\n"
  "paddsw %%xmm0,%%xmm2\n"
  "paddsw %%xmm0,%%xmm3\n"
  "shufps $0x44,%%xmm3,%%xmm2\n"
  "psraw  $0x6,%%xmm2\n"
  "packuswb %%xmm2,%%xmm2\n"
  "movq   %%xmm2,0x0(%3)\n"
  "add    $0x8,%3\n"
"convertend:"
  "sub    $0x2,%4\n"
  "jns    convertloop\n"

"convertnext:"
  "add    $0x1,%4\n"
  "js     convertdone\n"

  "movzb  (%1),%%r10\n"
  "movq   2048(%5,%%r10,8),%%xmm0\n"
  "movzb  (%2),%%r10\n"
  "movq   4096(%5,%%r10,8),%%xmm1\n"
  "paddsw %%xmm1,%%xmm0\n"
  "movzb  (%0),%%r10\n"
  "movq   (%5,%%r10,8),%%xmm1\n"
  "paddsw %%xmm0,%%xmm1\n"
  "psraw  $0x6,%%xmm1\n"
  "packuswb %%xmm1,%%xmm1\n"
  "movd   %%xmm1,0x0(%3)\n"
"convertdone:"
  :
  : "r"(y_buf),  // %0
    "r"(u_buf),  // %1
    "r"(v_buf),  // %2
    "r"(rgb_buf),  // %3
    "r"(width),  // %4
    "r" (kCoefficientsRgbY)  // %5
  : "memory", "r10", "r11", "xmm0", "xmm1", "xmm2", "xmm3"
);
}

void ScaleYUVToRGB32Row(const uint8* y_buf,  // rdi
                        const uint8* u_buf,  // rsi
                        const uint8* v_buf,  // rdx
                        uint8* rgb_buf,      // rcx
                        int width,           // r8
                        int source_dx) {     // r9
  asm(
  "xor    %%r11,%%r11\n"
  "sub    $0x2,%4\n"
  "js     scalenext\n"

"scaleloop:"
  "mov    %%r11,%%r10\n"
  "sar    $0x11,%%r10\n"
  "movzb  (%1,%%r10,1),%%rax\n"
  "movq   2048(%5,%%rax,8),%%xmm0\n"
  "movzb  (%2,%%r10,1),%%rax\n"
  "movq   4096(%5,%%rax,8),%%xmm1\n"
  "lea    (%%r11,%6),%%r10\n"
  "sar    $0x10,%%r11\n"
  "movzb  (%0,%%r11,1),%%rax\n"
  "paddsw %%xmm1,%%xmm0\n"
  "movq   (%5,%%rax,8),%%xmm1\n"
  "lea    (%%r10,%6),%%r11\n"
  "sar    $0x10,%%r10\n"
  "movzb  (%0,%%r10,1),%%rax\n"
  "movq   (%5,%%rax,8),%%xmm2\n"
  "paddsw %%xmm0,%%xmm1\n"
  "paddsw %%xmm0,%%xmm2\n"
  "shufps $0x44,%%xmm2,%%xmm1\n"
  "psraw  $0x6,%%xmm1\n"
  "packuswb %%xmm1,%%xmm1\n"
  "movq   %%xmm1,0x0(%3)\n"
  "add    $0x8,%3\n"
  "sub    $0x2,%4\n"
  "jns    scaleloop\n"

"scalenext:"
  "add    $0x1,%4\n"
  "js     scaledone\n"

  "mov    %%r11,%%r10\n"
  "sar    $0x11,%%r10\n"
  "movzb  (%1,%%r10,1),%%rax\n"
  "movq   2048(%5,%%rax,8),%%xmm0\n"
  "movzb  (%2,%%r10,1),%%rax\n"
  "movq   4096(%5,%%rax,8),%%xmm1\n"
  "paddsw %%xmm1,%%xmm0\n"
  "sar    $0x10,%%r11\n"
  "movzb  (%0,%%r11,1),%%rax\n"
  "movq   (%5,%%rax,8),%%xmm1\n"
  "paddsw %%xmm0,%%xmm1\n"
  "psraw  $0x6,%%xmm1\n"
  "packuswb %%xmm1,%%xmm1\n"
  "movd   %%xmm1,0x0(%3)\n"

"scaledone:"
  :
  : "r"(y_buf),  // %0
    "r"(u_buf),  // %1
    "r"(v_buf),  // %2
    "r"(rgb_buf),  // %3
    "r"(width),  // %4
    "r" (kCoefficientsRgbY),  // %5
    "r"(static_cast<long>(source_dx))  // %6
  : "memory", "r10", "r11", "rax", "xmm0", "xmm1", "xmm2"
);
}

void LinearScaleYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width,
                              int source_dx) {
  asm(
  "xor    %%r11,%%r11\n"   // x = 0
  "sub    $0x2,%4\n"
  "js     .lscalenext\n"
  "cmp    $0x20000,%6\n"   // if source_dx >= 2.0
  "jl     .lscalehalf\n"
  "mov    $0x8000,%%r11\n" // x = 0.5 for 1/2 or less
".lscalehalf:"

".lscaleloop:"
  "mov    %%r11,%%r10\n"
  "sar    $0x11,%%r10\n"

  "movzb  (%1, %%r10, 1), %%r13 \n"
  "movzb  1(%1, %%r10, 1), %%r14 \n"
  "mov    %%r11, %%rax \n"
  "and    $0x1fffe, %%rax \n"
  "imul   %%rax, %%r14 \n"
  "xor    $0x1fffe, %%rax \n"
  "imul   %%rax, %%r13 \n"
  "add    %%r14, %%r13 \n"
  "shr    $17, %%r13 \n"
  "movq   2048(%5,%%r13,8), %%xmm0\n"

  "movzb  (%2, %%r10, 1), %%r13 \n"
  "movzb  1(%2, %%r10, 1), %%r14 \n"
  "mov    %%r11, %%rax \n"
  "and    $0x1fffe, %%rax \n"
  "imul   %%rax, %%r14 \n"
  "xor    $0x1fffe, %%rax \n"
  "imul   %%rax, %%r13 \n"
  "add    %%r14, %%r13 \n"
  "shr    $17, %%r13 \n"
  "movq   4096(%5,%%r13,8), %%xmm1\n"

  "mov    %%r11, %%rax \n"
  "lea    (%%r11,%6),%%r10\n"
  "sar    $0x10,%%r11\n"
  "paddsw %%xmm1,%%xmm0\n"

  "movzb  (%0, %%r11, 1), %%r13 \n"
  "movzb  1(%0, %%r11, 1), %%r14 \n"
  "and    $0xffff, %%rax \n"
  "imul   %%rax, %%r14 \n"
  "xor    $0xffff, %%rax \n"
  "imul   %%rax, %%r13 \n"
  "add    %%r14, %%r13 \n"
  "shr    $16, %%r13 \n"
  "movq   (%5,%%r13,8),%%xmm1\n"

  "mov    %%r10, %%rax \n"
  "lea    (%%r10,%6),%%r11\n"
  "sar    $0x10,%%r10\n"

  "movzb  (%0,%%r10,1), %%r13 \n"
  "movzb  1(%0,%%r10,1), %%r14 \n"
  "and    $0xffff, %%rax \n"
  "imul   %%rax, %%r14 \n"
  "xor    $0xffff, %%rax \n"
  "imul   %%rax, %%r13 \n"
  "add    %%r14, %%r13 \n"
  "shr    $16, %%r13 \n"
  "movq   (%5,%%r13,8),%%xmm2\n"

  "paddsw %%xmm0,%%xmm1\n"
  "paddsw %%xmm0,%%xmm2\n"
  "shufps $0x44,%%xmm2,%%xmm1\n"
  "psraw  $0x6,%%xmm1\n"
  "packuswb %%xmm1,%%xmm1\n"
  "movq   %%xmm1,0x0(%3)\n"
  "add    $0x8,%3\n"
  "sub    $0x2,%4\n"
  "jns    .lscaleloop\n"

".lscalenext:"
  "add    $0x1,%4\n"
  "js     .lscaledone\n"

  "mov    %%r11,%%r10\n"
  "sar    $0x11,%%r10\n"

  "movzb  (%1,%%r10,1), %%r13 \n"
  "movq   2048(%5,%%r13,8),%%xmm0\n"

  "movzb  (%2,%%r10,1), %%r13 \n"
  "movq   4096(%5,%%r13,8),%%xmm1\n"

  "paddsw %%xmm1,%%xmm0\n"
  "sar    $0x10,%%r11\n"

  "movzb  (%0,%%r11,1), %%r13 \n"
  "movq   (%5,%%r13,8),%%xmm1\n"

  "paddsw %%xmm0,%%xmm1\n"
  "psraw  $0x6,%%xmm1\n"
  "packuswb %%xmm1,%%xmm1\n"
  "movd   %%xmm1,0x0(%3)\n"

".lscaledone:"
  :
  : "r"(y_buf),  // %0
    "r"(u_buf),  // %1
    "r"(v_buf),  // %2
    "r"(rgb_buf),  // %3
    "r"(width),  // %4
    "r" (kCoefficientsRgbY),  // %5
    "r"(static_cast<long>(source_dx))  // %6
  : "memory", "r10", "r11", "r13", "r14", "rax", "xmm0", "xmm1", "xmm2"
);
}

#elif USE_MMX && !defined(ARCH_CPU_X86_64) && !defined(__PIC__)

// PIC version is slower because less registers are available, so
// non-PIC is used on platforms where it is possible.

void FastConvertYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width);
  asm(
  ".text\n"
  ".global FastConvertYUVToRGB32Row\n"
"FastConvertYUVToRGB32Row:\n"
  "pusha\n"
  "mov    0x24(%esp),%edx\n"
  "mov    0x28(%esp),%edi\n"
  "mov    0x2c(%esp),%esi\n"
  "mov    0x30(%esp),%ebp\n"
  "mov    0x34(%esp),%ecx\n"
  "jmp    convertend\n"

"convertloop:"
  "movzbl (%edi),%eax\n"
  "add    $0x1,%edi\n"
  "movzbl (%esi),%ebx\n"
  "add    $0x1,%esi\n"
  "movq   kCoefficientsRgbY+2048(,%eax,8),%mm0\n"
  "movzbl (%edx),%eax\n"
  "paddsw kCoefficientsRgbY+4096(,%ebx,8),%mm0\n"
  "movzbl 0x1(%edx),%ebx\n"
  "movq   kCoefficientsRgbY(,%eax,8),%mm1\n"
  "add    $0x2,%edx\n"
  "movq   kCoefficientsRgbY(,%ebx,8),%mm2\n"
  "paddsw %mm0,%mm1\n"
  "paddsw %mm0,%mm2\n"
  "psraw  $0x6,%mm1\n"
  "psraw  $0x6,%mm2\n"
  "packuswb %mm2,%mm1\n"
  "movntq %mm1,0x0(%ebp)\n"
  "add    $0x8,%ebp\n"
"convertend:"
  "sub    $0x2,%ecx\n"
  "jns    convertloop\n"

  "and    $0x1,%ecx\n"
  "je     convertdone\n"

  "movzbl (%edi),%eax\n"
  "movq   kCoefficientsRgbY+2048(,%eax,8),%mm0\n"
  "movzbl (%esi),%eax\n"
  "paddsw kCoefficientsRgbY+4096(,%eax,8),%mm0\n"
  "movzbl (%edx),%eax\n"
  "movq   kCoefficientsRgbY(,%eax,8),%mm1\n"
  "paddsw %mm0,%mm1\n"
  "psraw  $0x6,%mm1\n"
  "packuswb %mm1,%mm1\n"
  "movd   %mm1,0x0(%ebp)\n"
"convertdone:"
  "popa\n"
  "ret\n"
);


void ScaleYUVToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width,
                        int source_dx);
  asm(
  ".text\n"
  ".global ScaleYUVToRGB32Row\n"
"ScaleYUVToRGB32Row:\n"
  "pusha\n"
  "mov    0x24(%esp),%edx\n"
  "mov    0x28(%esp),%edi\n"
  "mov    0x2c(%esp),%esi\n"
  "mov    0x30(%esp),%ebp\n"
  "mov    0x34(%esp),%ecx\n"
  "xor    %ebx,%ebx\n"
  "jmp    scaleend\n"

"scaleloop:"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%edi,%eax,1),%eax\n"
  "movq   kCoefficientsRgbY+2048(,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%esi,%eax,1),%eax\n"
  "paddsw kCoefficientsRgbY+4096(,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%eax\n"
  "movq   kCoefficientsRgbY(,%eax,8),%mm1\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%eax\n"
  "movq   kCoefficientsRgbY(,%eax,8),%mm2\n"
  "paddsw %mm0,%mm1\n"
  "paddsw %mm0,%mm2\n"
  "psraw  $0x6,%mm1\n"
  "psraw  $0x6,%mm2\n"
  "packuswb %mm2,%mm1\n"
  "movntq %mm1,0x0(%ebp)\n"
  "add    $0x8,%ebp\n"
"scaleend:"
  "sub    $0x2,%ecx\n"
  "jns    scaleloop\n"

  "and    $0x1,%ecx\n"
  "je     scaledone\n"

  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%edi,%eax,1),%eax\n"
  "movq   kCoefficientsRgbY+2048(,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%esi,%eax,1),%eax\n"
  "paddsw kCoefficientsRgbY+4096(,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%eax\n"
  "movq   kCoefficientsRgbY(,%eax,8),%mm1\n"
  "paddsw %mm0,%mm1\n"
  "psraw  $0x6,%mm1\n"
  "packuswb %mm1,%mm1\n"
  "movd   %mm1,0x0(%ebp)\n"

"scaledone:"
  "popa\n"
  "ret\n"
);

void LinearScaleYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width,
                              int source_dx);
  asm(
  ".text\n"
  ".global LinearScaleYUVToRGB32Row\n"
"LinearScaleYUVToRGB32Row:\n"
  "pusha\n"
  "mov    0x24(%esp),%edx\n"
  "mov    0x28(%esp),%edi\n"
  "mov    0x30(%esp),%ebp\n"

  // source_width = width * source_dx + ebx
  "mov    0x34(%esp), %ecx\n"
  "imull  0x38(%esp), %ecx\n"
  "mov    %ecx, 0x34(%esp)\n"

  "mov    0x38(%esp), %ecx\n"
  "xor    %ebx,%ebx\n"     // x = 0
  "cmp    $0x20000,%ecx\n" // if source_dx >= 2.0
  "jl     .lscaleend\n"
  "mov    $0x8000,%ebx\n"  // x = 0.5 for 1/2 or less
  "jmp    .lscaleend\n"

".lscaleloop:"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"

  "movzbl (%edi,%eax,1),%ecx\n"
  "movzbl 1(%edi,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "andl   $0x1fffe, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0x1fffe, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $17, %ecx \n"
  "movq   kCoefficientsRgbY+2048(,%ecx,8),%mm0\n"

  "mov    0x2c(%esp),%esi\n"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"

  "movzbl (%esi,%eax,1),%ecx\n"
  "movzbl 1(%esi,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "andl   $0x1fffe, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0x1fffe, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $17, %ecx \n"
  "paddsw kCoefficientsRgbY+4096(,%ecx,8),%mm0\n"

  "mov    %ebx,%eax\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%ecx\n"
  "movzbl 1(%edx,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "andl   $0xffff, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0xffff, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $16, %ecx \n"
  "movq   kCoefficientsRgbY(,%ecx,8),%mm1\n"

  "cmp    0x34(%esp), %ebx\n"
  "jge    .lscalelastpixel\n"

  "mov    %ebx,%eax\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%ecx\n"
  "movzbl 1(%edx,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "andl   $0xffff, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0xffff, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $16, %ecx \n"
  "movq   kCoefficientsRgbY(,%ecx,8),%mm2\n"

  "paddsw %mm0,%mm1\n"
  "paddsw %mm0,%mm2\n"
  "psraw  $0x6,%mm1\n"
  "psraw  $0x6,%mm2\n"
  "packuswb %mm2,%mm1\n"
  "movntq %mm1,0x0(%ebp)\n"
  "add    $0x8,%ebp\n"

".lscaleend:"
  "cmp    0x34(%esp), %ebx\n"
  "jl     .lscaleloop\n"
  "popa\n"
  "ret\n"

".lscalelastpixel:"
  "paddsw %mm0, %mm1\n"
  "psraw $6, %mm1\n"
  "packuswb %mm1, %mm1\n"
  "movd %mm1, (%ebp)\n"
  "popa\n"
  "ret\n"
);

#elif USE_MMX && !defined(ARCH_CPU_X86_64) && defined(__PIC__)

extern void PICConvertYUVToRGB32Row(const uint8* y_buf,
                                    const uint8* u_buf,
                                    const uint8* v_buf,
                                    uint8* rgb_buf,
                                    int width,
                                    int16 *kCoefficientsRgbY);
  asm(
  ".text\n"
#if defined(OS_MACOSX)
"_PICConvertYUVToRGB32Row:\n"
#else
"PICConvertYUVToRGB32Row:\n"
#endif
  "pusha\n"
  "mov    0x24(%esp),%edx\n"
  "mov    0x28(%esp),%edi\n"
  "mov    0x2c(%esp),%esi\n"
  "mov    0x30(%esp),%ebp\n"
  "mov    0x38(%esp),%ecx\n"

  "jmp    .Lconvertend\n"

".Lconvertloop:"
  "movzbl (%edi),%eax\n"
  "add    $0x1,%edi\n"
  "movzbl (%esi),%ebx\n"
  "add    $0x1,%esi\n"
  "movq   2048(%ecx,%eax,8),%mm0\n"
  "movzbl (%edx),%eax\n"
  "paddsw 4096(%ecx,%ebx,8),%mm0\n"
  "movzbl 0x1(%edx),%ebx\n"
  "movq   0(%ecx,%eax,8),%mm1\n"
  "add    $0x2,%edx\n"
  "movq   0(%ecx,%ebx,8),%mm2\n"
  "paddsw %mm0,%mm1\n"
  "paddsw %mm0,%mm2\n"
  "psraw  $0x6,%mm1\n"
  "psraw  $0x6,%mm2\n"
  "packuswb %mm2,%mm1\n"
  "movntq %mm1,0x0(%ebp)\n"
  "add    $0x8,%ebp\n"
".Lconvertend:"
  "subl   $0x2,0x34(%esp)\n"
  "jns    .Lconvertloop\n"

  "andl   $0x1,0x34(%esp)\n"
  "je     .Lconvertdone\n"

  "movzbl (%edi),%eax\n"
  "movq   2048(%ecx,%eax,8),%mm0\n"
  "movzbl (%esi),%eax\n"
  "paddsw 4096(%ecx,%eax,8),%mm0\n"
  "movzbl (%edx),%eax\n"
  "movq   0(%ecx,%eax,8),%mm1\n"
  "paddsw %mm0,%mm1\n"
  "psraw  $0x6,%mm1\n"
  "packuswb %mm1,%mm1\n"
  "movd   %mm1,0x0(%ebp)\n"
".Lconvertdone:\n"
  "popa\n"
  "ret\n"
);

void FastConvertYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width) {
  PICConvertYUVToRGB32Row(y_buf, u_buf, v_buf, rgb_buf, width,
                          &kCoefficientsRgbY[0][0]);
}

extern void PICScaleYUVToRGB32Row(const uint8* y_buf,
                               const uint8* u_buf,
                               const uint8* v_buf,
                               uint8* rgb_buf,
                               int width,
                               int source_dx,
                               int16 *kCoefficientsRgbY);

  asm(
  ".text\n"
#if defined(OS_MACOSX)
"_PICScaleYUVToRGB32Row:\n"
#else
"PICScaleYUVToRGB32Row:\n"
#endif
  "pusha\n"
  "mov    0x24(%esp),%edx\n"
  "mov    0x28(%esp),%edi\n"
  "mov    0x2c(%esp),%esi\n"
  "mov    0x30(%esp),%ebp\n"
  "mov    0x3c(%esp),%ecx\n"
  "xor    %ebx,%ebx\n"
  "jmp    Lscaleend\n"

"Lscaleloop:"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%edi,%eax,1),%eax\n"
  "movq   2048(%ecx,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%esi,%eax,1),%eax\n"
  "paddsw 4096(%ecx,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%eax\n"
  "movq   0(%ecx,%eax,8),%mm1\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%eax\n"
  "movq   0(%ecx,%eax,8),%mm2\n"
  "paddsw %mm0,%mm1\n"
  "paddsw %mm0,%mm2\n"
  "psraw  $0x6,%mm1\n"
  "psraw  $0x6,%mm2\n"
  "packuswb %mm2,%mm1\n"
  "movntq %mm1,0x0(%ebp)\n"
  "add    $0x8,%ebp\n"
"Lscaleend:"
  "subl   $0x2,0x34(%esp)\n"
  "jns    Lscaleloop\n"

  "andl   $0x1,0x34(%esp)\n"
  "je     Lscaledone\n"

  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%edi,%eax,1),%eax\n"
  "movq   2048(%ecx,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%esi,%eax,1),%eax\n"
  "paddsw 4096(%ecx,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%eax\n"
  "movq   0(%ecx,%eax,8),%mm1\n"
  "paddsw %mm0,%mm1\n"
  "psraw  $0x6,%mm1\n"
  "packuswb %mm1,%mm1\n"
  "movd   %mm1,0x0(%ebp)\n"

"Lscaledone:"
  "popa\n"
  "ret\n"
);


void ScaleYUVToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width,
                        int source_dx) {
  PICScaleYUVToRGB32Row(y_buf, u_buf, v_buf, rgb_buf, width, source_dx,
                        &kCoefficientsRgbY[0][0]);
}

void PICLinearScaleYUVToRGB32Row(const uint8* y_buf,
                                 const uint8* u_buf,
                                 const uint8* v_buf,
                                 uint8* rgb_buf,
                                 int width,
                                 int source_dx,
                                 int16 *kCoefficientsRgbY);
  asm(
  ".text\n"
#if defined(OS_MACOSX)
"_PICLinearScaleYUVToRGB32Row:\n"
#else
"PICLinearScaleYUVToRGB32Row:\n"
#endif
  "pusha\n"
  "mov    0x24(%esp),%edx\n"
  "mov    0x30(%esp),%ebp\n"
  "mov    0x34(%esp),%ecx\n"
  "mov    0x3c(%esp),%edi\n"
  "xor    %ebx,%ebx\n"

  // source_width = width * source_dx + ebx
  "mov    0x34(%esp), %ecx\n"
  "imull  0x38(%esp), %ecx\n"
  "mov    %ecx, 0x34(%esp)\n"

  "mov    0x38(%esp), %ecx\n"
  "xor    %ebx,%ebx\n"     // x = 0
  "cmp    $0x20000,%ecx\n" // if source_dx >= 2.0
  "jl     .lscaleend\n"
  "mov    $0x8000,%ebx\n"  // x = 0.5 for 1/2 or less
  "jmp    .lscaleend\n"

".lscaleloop:"
  "mov    0x28(%esp),%esi\n"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"

  "movzbl (%esi,%eax,1),%ecx\n"
  "movzbl 1(%esi,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "andl   $0x1fffe, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0x1fffe, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $17, %ecx \n"
  "movq   2048(%edi,%ecx,8),%mm0\n"

  "mov    0x2c(%esp),%esi\n"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"

  "movzbl (%esi,%eax,1),%ecx\n"
  "movzbl 1(%esi,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "andl   $0x1fffe, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0x1fffe, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $17, %ecx \n"
  "paddsw 4096(%edi,%ecx,8),%mm0\n"

  "mov    %ebx,%eax\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%ecx\n"
  "movzbl 1(%edx,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "andl   $0xffff, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0xffff, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $16, %ecx \n"
  "movq   (%edi,%ecx,8),%mm1\n"

  "cmp    0x34(%esp), %ebx\n"
  "jge    .lscalelastpixel\n"

  "mov    %ebx,%eax\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%ecx\n"
  "movzbl 1(%edx,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "andl   $0xffff, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0xffff, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $16, %ecx \n"
  "movq   (%edi,%ecx,8),%mm2\n"

  "paddsw %mm0,%mm1\n"
  "paddsw %mm0,%mm2\n"
  "psraw  $0x6,%mm1\n"
  "psraw  $0x6,%mm2\n"
  "packuswb %mm2,%mm1\n"
  "movntq %mm1,0x0(%ebp)\n"
  "add    $0x8,%ebp\n"

".lscaleend:"
  "cmp    %ebx, 0x34(%esp)\n"
  "jg     .lscaleloop\n"
  "popa\n"
  "ret\n"

".lscalelastpixel:"
  "paddsw %mm0, %mm1\n"
  "psraw $6, %mm1\n"
  "packuswb %mm1, %mm1\n"
  "movd %mm1, (%ebp)\n"
  "popa\n"
  "ret\n"
);

void LinearScaleYUVToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width,
                        int source_dx) {
  PICLinearScaleYUVToRGB32Row(y_buf, u_buf, v_buf, rgb_buf, width, source_dx,
                              &kCoefficientsRgbY[0][0]);
}

#else  // USE_MMX

// C reference code that mimic the YUV assembly.
#define packuswb(x) ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))
#define paddsw(x, y) (((x) + (y)) < -32768 ? -32768 : \
    (((x) + (y)) > 32767 ? 32767 : ((x) + (y))))

static inline void YuvPixel(uint8 y,
                            uint8 u,
                            uint8 v,
                            uint8* rgb_buf) {

  int b = kCoefficientsRgbY[256+u][0];
  int g = kCoefficientsRgbY[256+u][1];
  int r = kCoefficientsRgbY[256+u][2];
  int a = kCoefficientsRgbY[256+u][3];

  b = paddsw(b, kCoefficientsRgbY[512+v][0]);
  g = paddsw(g, kCoefficientsRgbY[512+v][1]);
  r = paddsw(r, kCoefficientsRgbY[512+v][2]);
  a = paddsw(a, kCoefficientsRgbY[512+v][3]);

  b = paddsw(b, kCoefficientsRgbY[y][0]);
  g = paddsw(g, kCoefficientsRgbY[y][1]);
  r = paddsw(r, kCoefficientsRgbY[y][2]);
  a = paddsw(a, kCoefficientsRgbY[y][3]);

  b >>= 6;
  g >>= 6;
  r >>= 6;
  a >>= 6;

  *reinterpret_cast<uint32*>(rgb_buf) = (packuswb(b)) |
                                        (packuswb(g) << 8) |
                                        (packuswb(r) << 16) |
                                        (packuswb(a) << 24);
}

void FastConvertYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width) {
  for (int x = 0; x < width; x += 2) {
    uint8 u = u_buf[x >> 1];
    uint8 v = v_buf[x >> 1];
    uint8 y0 = y_buf[x];
    YuvPixel(y0, u, v, rgb_buf);
    if ((x + 1) < width) {
      uint8 y1 = y_buf[x + 1];
      YuvPixel(y1, u, v, rgb_buf + 4);
    }
    rgb_buf += 8;  // Advance 2 pixels.
  }
}

// 16.16 fixed point is used.  A shift by 16 isolates the integer.
// A shift by 17 is used to further subsample the chrominence channels.
// & 0xffff isolates the fixed point fraction.  >> 2 to get the upper 2 bits,
// for 1/65536 pixel accurate interpolation.
void ScaleYUVToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width,
                        int source_dx) {
  int x = 0;
  for (int i = 0; i < width; i += 2) {
    int y = y_buf[x >> 16];
    int u = u_buf[(x >> 17)];
    int v = v_buf[(x >> 17)];
    YuvPixel(y, u, v, rgb_buf);
    x += source_dx;
    if ((i + 1) < width) {
      y = y_buf[x >> 16];
      YuvPixel(y, u, v, rgb_buf+4);
      x += source_dx;
    }
    rgb_buf += 8;
  }
}

void LinearScaleYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width,
                              int source_dx) {
  int x = 0;
  if (source_dx >= 0x20000) {
    x = 32768;
  }
  for (int i = 0; i < width; i += 2) {
    int y0 = y_buf[x >> 16];
    int y1 = y_buf[(x >> 16) + 1];
    int u0 = u_buf[(x >> 17)];
    int u1 = u_buf[(x >> 17) + 1];
    int v0 = v_buf[(x >> 17)];
    int v1 = v_buf[(x >> 17) + 1];
    int y_frac = (x & 65535);
    int uv_frac = ((x >> 1) & 65535);
    int y = (y_frac * y1 + (y_frac ^ 65535) * y0) >> 16;
    int u = (uv_frac * u1 + (uv_frac ^ 65535) * u0) >> 16;
    int v = (uv_frac * v1 + (uv_frac ^ 65535) * v0) >> 16;
    YuvPixel(y, u, v, rgb_buf);
    x += source_dx;
    if ((i + 1) < width) {
      y0 = y_buf[x >> 16];
      y1 = y_buf[(x >> 16) + 1];
      y_frac = (x & 65535);
      y = (y_frac * y1 + (y_frac ^ 65535) * y0) >> 16;
      YuvPixel(y, u, v, rgb_buf+4);
      x += source_dx;
    }
    rgb_buf += 8;
  }
}

#endif  // USE_MMX
}  // extern "C"

