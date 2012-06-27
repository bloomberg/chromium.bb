// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_CRASH_REPORTING_VEH_TEST_H_
#define CHROME_FRAME_CRASH_REPORTING_VEH_TEST_H_

#include <windows.h>
#include "base/logging.h"

#ifndef EXCEPTION_CHAIN_END
#define EXCEPTION_CHAIN_END ((struct _EXCEPTION_REGISTRATION_RECORD*)-1)
#if !defined(_WIN32_WINNT_WIN8)
typedef struct _EXCEPTION_REGISTRATION_RECORD {
  struct _EXCEPTION_REGISTRATION_RECORD* Next;
  PVOID Handler;
} EXCEPTION_REGISTRATION_RECORD;
// VEH handler flags settings.
// These are grabbed from winnt.h for PocketPC.
// Only EXCEPTION_NONCONTINUABLE in defined in "regular" winnt.h
// #define EXCEPTION_NONCONTINUABLE 0x1    // Noncontinuable exception
#define EXCEPTION_UNWINDING 0x2         // Unwind is in progress
#define EXCEPTION_EXIT_UNWIND 0x4       // Exit unwind is in progress
#define EXCEPTION_STACK_INVALID 0x8     // Stack out of limits or unaligned
#define EXCEPTION_NESTED_CALL 0x10      // Nested exception handler call
#define EXCEPTION_TARGET_UNWIND 0x20    // Target unwind in progress
#define EXCEPTION_COLLIDED_UNWIND 0x40  // Collided exception handler call

#define EXCEPTION_UNWIND (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND | \
    EXCEPTION_TARGET_UNWIND | EXCEPTION_COLLIDED_UNWIND)

#define IS_UNWINDING(Flag)  (((Flag) & EXCEPTION_UNWIND) != 0)
#define IS_DISPATCHING(Flag)  (((Flag) & EXCEPTION_UNWIND) == 0)
#define IS_TARGET_UNWIND(Flag)  ((Flag) & EXCEPTION_TARGET_UNWIND)
#endif  // !defined(_WIN32_WINNT_WIN8)
#endif  // EXCEPTION_CHAIN_END

class ExceptionInfo : public _EXCEPTION_POINTERS {
 public:
  ExceptionInfo() {
    Clear();
  }

  ExceptionInfo(DWORD code, void* address) {
    Clear();
    Set(code, address, 0);
  }

  void Set(DWORD code, void* address, DWORD flags) {
    er_.ExceptionCode = code;
    er_.ExceptionAddress = address;
    er_.ExceptionFlags = flags;
    ctx_.Eip = reinterpret_cast<DWORD>(address);
  }

  EXCEPTION_RECORD er_;
  CONTEXT ctx_;
 private:
  void Clear() {
    ExceptionRecord = &er_;
    ContextRecord = &ctx_;
    ZeroMemory(&er_, sizeof(er_));
    ZeroMemory(&ctx_, sizeof(ctx_));
  }
};

struct SEHChain {
  SEHChain(const void* p, ...) {
    va_list vl;
    va_start(vl, p);
    int i = 0;
    for (; p; ++i) {
      CHECK(i + 1 < arraysize(chain_));
      chain_[i].Handler =
          reinterpret_cast<PEXCEPTION_ROUTINE>(const_cast<void*>(p));
      chain_[i].Next = &chain_[i + 1];
      p = va_arg(vl, const void*);
    }

    chain_[i].Next = EXCEPTION_CHAIN_END;
  }

  EXCEPTION_REGISTRATION_RECORD chain_[25];
};

struct StackHelper {
  StackHelper(const void* p, ...) {
    va_list vl;
    va_start(vl, p);
    count_ = 0;
    for (; p; ++count_) {
      CHECK(count_ < arraysize(stack_));
      stack_[count_] = p;
      p = va_arg(vl, const void*);
    }
  }
  const void* stack_[64];
  WORD count_;
};

#endif  // CHROME_FRAME_CRASH_REPORTING_VEH_TEST_H_
