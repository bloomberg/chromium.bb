// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Code in this file is taked from
// third_party/skia/src/opts/opts_check_SSE2.cpp.

// Note that this file cannot be compiled with -msse2 in gcc.

#include "build/build_config.h"
#include "media/base/cpu_features.h"

namespace media {

#ifdef _MSC_VER
static inline void getcpuid(int info_type, int info[4]) {
  __asm {
    mov    eax, [info_type]
        cpuid
        mov    edi, [info]
        mov    [edi], eax
        mov    [edi+4], ebx
        mov    [edi+8], ecx
        mov    [edi+12], edx
        }
}
#else
static inline void getcpuid(int info_type, int info[4]) {
  // We save and restore ebx, so this code can be compatible with -fPIC
#if defined(__i386__)
  asm volatile (
        "pushl %%ebx      \n\t"
        "cpuid            \n\t"
        "movl %%ebx, %1   \n\t"
        "popl %%ebx       \n\t"
        : "=a"(info[0]), "=r"(info[1]), "=c"(info[2]), "=d"(info[3])
        : "a"(info_type)
                );
#else
  // We can use cpuid instruction without saving ebx on gcc x86-64 because it
  // does not use ebx (or rbx) as a GOT register.
  asm volatile (
      "cpuid            \n\t"
      : "=a"(info[0]), "=r"(info[1]), "=c"(info[2]), "=d"(info[3])
      : "a"(info_type)
  );
#endif
}
#endif

bool hasMMX() {
  // TODO(hclam): Acutually checks it.
  return true;
}

bool hasSSE() {
  // TODO(hclam): Actually checks it.
  return true;
}

bool hasSSE2() {
#if defined(ARCH_CPU_X86_64)
  /* All x86_64 machines have SSE2, so don't even bother checking. */
  return true;
#else
  int cpu_info[4] = { 0 };
  getcpuid(1, cpu_info);
  return (cpu_info[3] & (1<<26)) != 0;
#endif
}

bool hasSSSE3() {
  int cpu_info[4] = { 0 };
  getcpuid(1, cpu_info);
  return (cpu_info[3] & 0x04000000) != 0 && (cpu_info[2] & 0x00000001) != 0 &&
      (cpu_info[2] & 0x00000200) != 0;
}

}  // namespace media
