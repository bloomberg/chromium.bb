// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_CRASH_REPORTING_VEH_TEST_H_
#define CHROME_FRAME_CRASH_REPORTING_VEH_TEST_H_

#include <windows.h>
#include "base/logging.h"

#ifndef EXCEPTION_CHAIN_END
#define EXCEPTION_CHAIN_END ((struct _EXCEPTION_REGISTRATION_RECORD*)-1)
typedef struct _EXCEPTION_REGISTRATION_RECORD {
  struct _EXCEPTION_REGISTRATION_RECORD* Next;
  PVOID Handler;
} EXCEPTION_REGISTRATION_RECORD;
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
      chain_[i].Handler = const_cast<void*>(p);
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