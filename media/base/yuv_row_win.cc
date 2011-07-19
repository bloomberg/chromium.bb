// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/yuv_row.h"

#define kCoefficientsRgbU kCoefficientsRgbY + 2048
#define kCoefficientsRgbV kCoefficientsRgbY + 4096

extern "C" {

#if USE_MMX
__declspec(naked)
void FastConvertYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    jmp       convertend

 convertloop :
    movzx     eax, byte ptr [edi]
    add       edi, 1
    movzx     ebx, byte ptr [esi]
    add       esi, 1
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [edx]
    paddsw    mm0, [kCoefficientsRgbV + 8 * ebx]
    movzx     ebx, byte ptr [edx + 1]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    add       edx, 2
    movq      mm2, [kCoefficientsRgbY + 8 * ebx]
    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 6
    psraw     mm2, 6
    packuswb  mm1, mm2
    movntq    [ebp], mm1
    add       ebp, 8
 convertend :
    sub       ecx, 2
    jns       convertloop

    and       ecx, 1  // odd number of pixels?
    jz        convertdone

    movzx     eax, byte ptr [edi]
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [esi]
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    movzx     eax, byte ptr [edx]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    movd      [ebp], mm1
 convertdone :

    popad
    ret
  }
}

__declspec(naked)
void ConvertYUVToRGB32Row(const uint8* y_buf,
                          const uint8* u_buf,
                          const uint8* v_buf,
                          uint8* rgb_buf,
                          int width,
                          int step) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    mov       ebx, [esp + 32 + 24]  // step
    jmp       wend

 wloop :
    movzx     eax, byte ptr [edi]
    add       edi, ebx
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [esi]
    add       esi, ebx
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    movzx     eax, byte ptr [edx]
    add       edx, ebx
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    movzx     eax, byte ptr [edx]
    add       edx, ebx
    movq      mm2, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 6
    psraw     mm2, 6
    packuswb  mm1, mm2
    movntq    [ebp], mm1
    add       ebp, 8
 wend :
    sub       ecx, 2
    jns       wloop

    and       ecx, 1  // odd number of pixels?
    jz        wdone

    movzx     eax, byte ptr [edi]
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [esi]
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    movzx     eax, byte ptr [edx]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    movd      [ebp], mm1
 wdone :

    popad
    ret
  }
}

__declspec(naked)
void RotateConvertYUVToRGB32Row(const uint8* y_buf,
                                const uint8* u_buf,
                                const uint8* v_buf,
                                uint8* rgb_buf,
                                int width,
                                int ystep,
                                int uvstep) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    jmp       wend

 wloop :
    movzx     eax, byte ptr [edi]
    mov       ebx, [esp + 32 + 28]  // uvstep
    add       edi, ebx
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [esi]
    add       esi, ebx
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    movzx     eax, byte ptr [edx]
    mov       ebx, [esp + 32 + 24]  // ystep
    add       edx, ebx
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    movzx     eax, byte ptr [edx]
    add       edx, ebx
    movq      mm2, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 6
    psraw     mm2, 6
    packuswb  mm1, mm2
    movntq    [ebp], mm1
    add       ebp, 8
 wend :
    sub       ecx, 2
    jns       wloop

    and       ecx, 1  // odd number of pixels?
    jz        wdone

    movzx     eax, byte ptr [edi]
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [esi]
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    movzx     eax, byte ptr [edx]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    movd      [ebp], mm1
 wdone :

    popad
    ret
  }
}

__declspec(naked)
void DoubleYUVToRGB32Row(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* rgb_buf,
                         int width) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    jmp       wend

 wloop :
    movzx     eax, byte ptr [edi]
    add       edi, 1
    movzx     ebx, byte ptr [esi]
    add       esi, 1
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [edx]
    paddsw    mm0, [kCoefficientsRgbV + 8 * ebx]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    punpckldq mm1, mm1
    movntq    [ebp], mm1

    movzx     ebx, byte ptr [edx + 1]
    add       edx, 2
    paddsw    mm0, [kCoefficientsRgbY + 8 * ebx]
    psraw     mm0, 6
    packuswb  mm0, mm0
    punpckldq mm0, mm0
    movntq    [ebp+8], mm0
    add       ebp, 16
 wend :
    sub       ecx, 4
    jns       wloop

    add       ecx, 4
    jz        wdone

    movzx     eax, byte ptr [edi]
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [esi]
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    movzx     eax, byte ptr [edx]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    jmp       wend1

 wloop1 :
    movd      [ebp], mm1
    add       ebp, 4
 wend1 :
    sub       ecx, 1
    jns       wloop1
 wdone :
    popad
    ret
  }
}

// This version does general purpose scaling by any amount, up or down.
// The only thing it can not do it rotation by 90 or 270.
// For performance the chroma is under sampled, reducing cost of a 3x
// 1080p scale from 8.4 ms to 5.4 ms.
__declspec(naked)
void ScaleYUVToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width,
                        int source_dx) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    xor       ebx, ebx              // x
    jmp       scaleend

 scaleloop :
    mov       eax, ebx
    sar       eax, 17
    movzx     eax, byte ptr [edi + eax]
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    mov       eax, ebx
    sar       eax, 17
    movzx     eax, byte ptr [esi + eax]
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    mov       eax, ebx
    add       ebx, [esp + 32 + 24]  // x += source_dx
    sar       eax, 16
    movzx     eax, byte ptr [edx + eax]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    mov       eax, ebx
    add       ebx, [esp + 32 + 24]  // x += source_dx
    sar       eax, 16
    movzx     eax, byte ptr [edx + eax]
    movq      mm2, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 6
    psraw     mm2, 6
    packuswb  mm1, mm2
    movntq    [ebp], mm1
    add       ebp, 8
 scaleend :
    sub       ecx, 2
    jns       scaleloop

    and       ecx, 1  // odd number of pixels?
    jz        scaledone

    mov       eax, ebx
    sar       eax, 17
    movzx     eax, byte ptr [edi + eax]
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    mov       eax, ebx
    sar       eax, 17
    movzx     eax, byte ptr [esi + eax]
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    mov       eax, ebx
    sar       eax, 16
    movzx     eax, byte ptr [edx + eax]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    movd      [ebp], mm1

 scaledone :
    popad
    ret
  }
}

__declspec(naked)
void LinearScaleYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width,
                              int source_dx) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]  // Y
    mov       edi, [esp + 32 + 8]  // U
                // [esp + 32 + 12] // V
    mov       ebp, [esp + 32 + 16] // rgb
    mov       ecx, [esp + 32 + 20] // width
    imul      ecx, [esp + 32 + 24] // source_dx
    mov       [esp + 32 + 20], ecx // source_width = width * source_dx
    mov       ecx, [esp + 32 + 24] // source_dx
    xor       ebx, ebx             // x = 0
    cmp       ecx, 0x20000
    jl        lscaleend
    mov       ebx, 0x8000          // x = 0.5 for 1/2 or less
    jmp       lscaleend
lscaleloop:
    mov       eax, ebx
    sar       eax, 0x11

    movzx     ecx, byte ptr [edi + eax]
    movzx     esi, byte ptr [edi + eax + 1]
    mov       eax, ebx
    and       eax, 0x1fffe
    imul      esi, eax
    xor       eax, 0x1fffe
    imul      ecx, eax
    add       ecx, esi
    shr       ecx, 17
    movq      mm0, [kCoefficientsRgbU + 8 * ecx]

    mov       esi, [esp + 32 + 12]
    mov       eax, ebx
    sar       eax, 0x11

    movzx     ecx, byte ptr [esi + eax]
    movzx     esi, byte ptr [esi + eax + 1]
    mov       eax, ebx
    and       eax, 0x1fffe
    imul      esi, eax
    xor       eax, 0x1fffe
    imul      ecx, eax
    add       ecx, esi
    shr       ecx, 17
    paddsw    mm0, [kCoefficientsRgbV + 8 * ecx]

    mov       eax, ebx
    sar       eax, 0x10
    movzx     ecx, byte ptr [edx + eax]
    movzx     esi, byte ptr [1 + edx + eax]
    mov       eax, ebx
    add       ebx, [esp + 32 + 24]
    and       eax, 0xffff
    imul      esi, eax
    xor       eax, 0xffff
    imul      ecx, eax
    add       ecx, esi
    shr       ecx, 16
    movq      mm1, [kCoefficientsRgbY + 8 * ecx]

    cmp       ebx, [esp + 32 + 20]
    jge       lscalelastpixel

    mov       eax, ebx
    sar       eax, 0x10
    movzx     ecx, byte ptr [edx + eax]
    movzx     esi, byte ptr [edx + eax + 1]
    mov       eax, ebx
    add       ebx, [esp + 32 + 24]
    and       eax, 0xffff
    imul      esi, eax
    xor       eax, 0xffff
    imul      ecx, eax
    add       ecx, esi
    shr       ecx, 16
    movq      mm2, [kCoefficientsRgbY + 8 * ecx]

    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 0x6
    psraw     mm2, 0x6
    packuswb  mm1, mm2
    movntq    [ebp], mm1
    add       ebp, 0x8

lscaleend:
    cmp       ebx, [esp + 32 + 20]
    jl        lscaleloop
    popad
    ret

lscalelastpixel:
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    movd      [ebp], mm1
    popad
    ret
  };
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

#if TEST_MMX_YUV
static inline void YuvPixel(uint8 y,
                            uint8 u,
                            uint8 v,
                            uint8* rgb_buf) {

  __asm {
    movzx     eax, u
    movq      mm0, [kCoefficientsRgbY+2048 + 8 * eax]
    movzx     eax, v
    paddsw    mm0, [kCoefficientsRgbY+4096 + 8 * eax]
    movzx     eax, y
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    mov       eax, rgb_buf
    movd      [eax], mm1
    emms
  }
}
#endif

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

