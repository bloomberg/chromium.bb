// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>

#include "base/logging.h"
#include "chrome_frame/crash_reporting/vectored_handler-impl.h"
#include "gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Class that mocks external call from VectoredHandlerT for testing purposes.
class EMock : public VEHTraitsBase {
 public:
  static inline bool WriteDump(EXCEPTION_POINTERS* p) {
    g_dump_made = true;
    return true;
  }

  static inline void* Register(PVECTORED_EXCEPTION_HANDLER func,
                               const void* module_start,
                               const void* module_end) {
    VEHTraitsBase::SetModule(module_start, module_end);
    // Return some arbitrary number, expecting to get the same on Unregister()
    return reinterpret_cast<void*>(4);
  }

  static inline ULONG Unregister(void* handle) {
    EXPECT_EQ(handle, reinterpret_cast<void*>(4));
    return 1;
  }

  static inline WORD RtlCaptureStackBackTrace(DWORD FramesToSkip,
      DWORD FramesToCapture, void** BackTrace, DWORD* BackTraceHash) {
    EXPECT_EQ(2, FramesToSkip);
    EXPECT_LE(FramesToSkip + FramesToCapture,
              VectoredHandlerBase::max_back_trace);
    memcpy(BackTrace, g_stack, g_stack_entries * sizeof(BackTrace[0]));
    return g_stack_entries;
  }

  static inline EXCEPTION_REGISTRATION_RECORD* RtlpGetExceptionList() {
    return g_seh_chain;
  }

  // Test helpers

  // Create fake SEH chain of random filters - with and without our module.
  static void SetHaveSEHFilter() {
    SetSEHChain(reinterpret_cast<const char*>(g_module_start) - 0x1000,
                reinterpret_cast<const char*>(g_module_start) + 0x1000,
                reinterpret_cast<const char*>(g_module_end) + 0x7127,
                NULL);
  }

  static void SetNoSEHFilter() {
    SetSEHChain(reinterpret_cast<const char*>(g_module_start) - 0x1000,
                reinterpret_cast<const char*>(g_module_end) + 0x7127,
                NULL);
  }

  // Create fake stack - with and without our module.
  static void SetOnStack() {
    SetStack(reinterpret_cast<const char*>(g_module_start) - 0x11283,
             reinterpret_cast<const char*>(g_module_start) - 0x278361,
             reinterpret_cast<const char*>(g_module_start) + 0x9171,
             reinterpret_cast<const char*>(g_module_end) + 1231,
             NULL);
  }

  static void SetNotOnStack() {
    SetStack(reinterpret_cast<const char*>(g_module_start) - 0x11283,
             reinterpret_cast<const char*>(g_module_start) - 0x278361,
             reinterpret_cast<const char*>(g_module_end) + 1231,
             NULL);
  }

  // Populate stack array
  static void SetStack(const void* p, ...) {
    va_list vl;
    va_start(vl, p);
    g_stack_entries = 0;
    for (; p; ++g_stack_entries) {
      CHECK(g_stack_entries < arraysize(g_stack));
      g_stack[g_stack_entries] = p;
      p = va_arg(vl, const void*);
    }
  }

  static void SetSEHChain(const void* p, ...) {
    va_list vl;
    va_start(vl, p);
    int i = 0;
    for (; p; ++i) {
      CHECK(i + 1 < arraysize(g_seh_chain));
      g_seh_chain[i].Handler = const_cast<void*>(p);
      g_seh_chain[i].Next = &g_seh_chain[i + 1];
      p = va_arg(vl, const void*);
    }

    g_seh_chain[i].Next = EXCEPTION_CHAIN_END;
  }

  static EXCEPTION_REGISTRATION_RECORD g_seh_chain[25];
  static const void* g_stack[VectoredHandlerBase::max_back_trace];
  static WORD g_stack_entries;
  static bool g_dump_made;
};

EXCEPTION_REGISTRATION_RECORD EMock::g_seh_chain[25];
const void* EMock::g_stack[VectoredHandlerBase::max_back_trace];
WORD EMock::g_stack_entries;
bool EMock::g_dump_made;

typedef VectoredHandlerT<EMock> VectoredHandlerMock;

class ExPtrsHelper : public _EXCEPTION_POINTERS {
 public:
  ExPtrsHelper() {
    ExceptionRecord = &er_;
    ContextRecord = &ctx_;
    ZeroMemory(&er_, sizeof(er_));
    ZeroMemory(&ctx_, sizeof(ctx_));
  }

  void Set(DWORD code, void* address, DWORD flags) {
    er_.ExceptionCode = code;
    er_.ExceptionAddress = address;
    er_.ExceptionFlags = flags;
  }

  EXCEPTION_RECORD er_;
  CONTEXT ctx_;
};


TEST(ChromeFrame, ExceptionReport) {
  char* s = reinterpret_cast<char*>(0x30000000);
  char* e = s + 0x10000;
  void* handler = VectoredHandlerMock::Register(s, e);
  char* our_code = s + 0x1111;
  char* not_our_code = s - 0x5555;
  char* not_our_code2 = e + 0x5555;

  ExPtrsHelper ex;
  // Exception in our code, but we have SEH filter
  ex.Set(STATUS_ACCESS_VIOLATION, our_code, 0);
  EMock::SetHaveSEHFilter();
  EMock::SetOnStack();
  EXPECT_EQ(ExceptionContinueSearch, VectoredHandlerMock::VectoredHandler(&ex));
  EXPECT_EQ(1, VectoredHandlerMock::g_exceptions_seen);
  EXPECT_FALSE(EMock::g_dump_made);

  // RPC_E_DISCONNECTED (0x80010108) is "The object invoked has disconnected
  // from its clients", shall not be caught since it's a warning only.
  ex.Set(RPC_E_DISCONNECTED, our_code, 0);
  EMock::SetHaveSEHFilter();
  EMock::SetOnStack();
  EXPECT_EQ(ExceptionContinueSearch, VectoredHandlerMock::VectoredHandler(&ex));
  EXPECT_EQ(1, VectoredHandlerMock::g_exceptions_seen);
  EXPECT_FALSE(EMock::g_dump_made);


  // Exception, not in our code, we do not have SEH and we are not in stack.
  ex.Set(STATUS_INTEGER_DIVIDE_BY_ZERO, not_our_code, 0);
  EMock::SetNoSEHFilter();
  EMock::SetNotOnStack();
  EXPECT_EQ(ExceptionContinueSearch, VectoredHandlerMock::VectoredHandler(&ex));
  EXPECT_EQ(2, VectoredHandlerMock::g_exceptions_seen);
  EXPECT_FALSE(EMock::g_dump_made);

  // Exception, not in our code, no SEH, but we are on the stack.
  ex.Set(STATUS_INTEGER_DIVIDE_BY_ZERO, not_our_code2, 0);
  EMock::SetNoSEHFilter();
  EMock::SetOnStack();
  EXPECT_EQ(ExceptionContinueSearch, VectoredHandlerMock::VectoredHandler(&ex));
  EXPECT_EQ(3, VectoredHandlerMock::g_exceptions_seen);
  EXPECT_TRUE(EMock::g_dump_made);
  EMock::g_dump_made = false;


  // Exception, in our code, no SEH, not on stack (assume FPO screwed us)
  ex.Set(STATUS_INTEGER_DIVIDE_BY_ZERO, our_code, 0);
  EMock::SetNoSEHFilter();
  EMock::SetNotOnStack();
  EXPECT_EQ(ExceptionContinueSearch, VectoredHandlerMock::VectoredHandler(&ex));
  EXPECT_EQ(4, VectoredHandlerMock::g_exceptions_seen);
  EXPECT_TRUE(EMock::g_dump_made);
  EMock::g_dump_made = false;

  VectoredHandlerMock::Unregister();
}
