;
; Copyright (c) 2016, Alliance for Open Media. All rights reserved
;
; This source code is subject to the terms of the BSD 2 Clause License and
; the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
; was not distributed with this source code in the LICENSE file, you can
; obtain it at www.aomedia.org/license/software. If the Alliance for Open
; Media Patent License 1.0 was not distributed with this source code in the
; PATENTS file, you can obtain it at www.aomedia.org/license/patent.
;

%include "aom_ports/x86_abi_support.asm"

;void aom_plane_add_noise_sse2(unsigned char *start, unsigned char *noise,
;                              unsigned char blackclamp[16],
;                              unsigned char whiteclamp[16],
;                              unsigned char bothclamp[16],
;                              unsigned int width, unsigned int height,
;                              int pitch)
global sym(aom_plane_add_noise_sse2) PRIVATE
sym(aom_plane_add_noise_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 8
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ; get the clamps in registers
    mov     rdx, arg(2) ; blackclamp
    movdqu  xmm3, [rdx]
    mov     rdx, arg(3) ; whiteclamp
    movdqu  xmm4, [rdx]
    mov     rdx, arg(4) ; bothclamp
    movdqu  xmm5, [rdx]

.addnoise_loop:
    call sym(LIBAOM_RAND) WRT_PLT
    mov     rcx, arg(1) ;noise
    and     rax, 0xff
    add     rcx, rax

    mov     rdi, rcx
    movsxd  rcx, dword arg(5) ;[Width]
    mov     rsi, arg(0) ;Pos
    xor         rax,rax

.addnoise_nextset:
      movdqu      xmm1,[rsi+rax]         ; get the source

      psubusb     xmm1, xmm3 ; subtract black clamp
      paddusb     xmm1, xmm5 ; add both clamp
      psubusb     xmm1, xmm4 ; subtract whiteclamp

      movdqu      xmm2,[rdi+rax]         ; get the noise for this line
      paddb       xmm1,xmm2              ; add it in
      movdqu      [rsi+rax],xmm1         ; store the result

      add         rax,16                 ; move to the next line

      cmp         rax, rcx
      jl          .addnoise_nextset

    movsxd  rax, dword arg(7) ; Pitch
    add     arg(0), rax ; Start += Pitch
    sub     dword arg(6), 1   ; Height -= 1
    jg      .addnoise_loop

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret

SECTION_RODATA
align 16
rd42:
    times 8 dw 0x04
four8s:
    times 4 dd 8
