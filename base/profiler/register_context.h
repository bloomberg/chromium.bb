// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides the RegisterContext cross-platform typedef that represents
// the native register context for the platform, plus functions that provide
// access to key registers in the context.

#ifndef BASE_PROFILER_REGISTER_CONTEXT_H_
#define BASE_PROFILER_REGISTER_CONTEXT_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_WIN)
using RegisterContext = ::CONTEXT;

inline uintptr_t& RegisterContextStackPointer(::CONTEXT* context) {
#if defined(ARCH_CPU_X86_64)
  return context->Rsp;
#elif defined(ARCH_CPU_ARM64)
  return context->Sp;
#else
  // The reinterpret_cast accounts for the fact that Esp is a DWORD, which is an
  // unsigned long, while uintptr_t is an unsigned int. The two types have the
  // same representation on Windows, but C++ treats them as different.
  return *reinterpret_cast<uintptr_t*>(&context->Esp);
#endif
}

inline uintptr_t& RegisterContextFramePointer(::CONTEXT* context) {
#if defined(ARCH_CPU_X86_64)
  return context->Rbp;
#elif defined(ARCH_CPU_ARM64)
  return context->Fp;
#else
  // The reinterpret_cast accounts for the fact that Ebp is a DWORD, which is an
  // unsigned long, while uintptr_t is an unsigned int. The two types have the
  // same representation on Windows, but C++ treats them as different.
  return *reinterpret_cast<uintptr_t*>(&context->Ebp);
#endif
}
#else   // #if defined(OS_WIN)
// Placeholders for other platforms.
struct RegisterContext {
  uintptr_t stack_pointer;
  uintptr_t frame_pointer;
};

inline uintptr_t& RegisterContextStackPointer(RegisterContext* context) {
  return context->stack_pointer;
}

inline uintptr_t& RegisterContextFramePointer(RegisterContext* context) {
  return context->frame_pointer;
}
#endif  // #if defined(OS_WIN)

#endif  // BASE_PROFILER_REGISTER_CONTEXT_H_
