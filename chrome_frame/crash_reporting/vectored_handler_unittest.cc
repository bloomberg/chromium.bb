// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>

#include "base/environment.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome_frame/crash_reporting/crash_dll.h"
#include "chrome_frame/crash_reporting/nt_loader.h"
#include "chrome_frame/crash_reporting/vectored_handler-impl.h"
#include "chrome_frame/crash_reporting/veh_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

ACTION_P(StackTraceDump, s) {
  memcpy(arg2, s->stack_, s->count_ * sizeof(s->stack_[0]));
  return s->count_;
}
namespace {
class MockApi : public Win32VEHTraits, public ModuleOfInterest {
 public:
  MockApi() {}

  MOCK_METHOD1(WriteDump, void(const EXCEPTION_POINTERS*));
  MOCK_METHOD0(RtlpGetExceptionList, const EXCEPTION_REGISTRATION_RECORD*());
  MOCK_METHOD4(RtlCaptureStackBackTrace, WORD(DWORD FramesToSkip,
      DWORD FramesToCapture, void** BackTrace, DWORD* BackTraceHash));

  // Helpers
  void SetSEH(const SEHChain& sehchain) {
    EXPECT_CALL(*this, RtlpGetExceptionList())
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(sehchain.chain_));
  }

  void SetStack(const StackHelper& s) {
    EXPECT_CALL(*this, RtlCaptureStackBackTrace(_, _, _, _))
        .Times(testing::AnyNumber())
        .WillRepeatedly(StackTraceDump(&s));
  }

  enum {max_back_trace = 15};
};
};  // namespace

typedef VectoredHandlerT<MockApi> VectoredHandlerMock;
static VectoredHandlerMock* g_mock_veh = NULL;
static LONG WINAPI VEH(EXCEPTION_POINTERS* exptrs) {
  return g_mock_veh->Handler(exptrs);
}

TEST(ChromeFrame, ExceptionReport) {
  MockApi api;
  VectoredHandlerMock veh(&api);

  // Start address of our module.
  char* s = reinterpret_cast<char*>(&__ImageBase);
  char *e = s + 0x10000;
  api.SetModule(s, e);

  SEHChain have_seh(s - 0x1000, s + 0x1000, e + 0x7127, NULL);
  SEHChain no_seh(s - 0x1000, e + 0x7127, NULL);
  StackHelper on_stack(s - 0x11283, s - 0x278361, s + 0x9171, e + 1231, NULL);
  StackHelper not_on_stack(s - 0x11283, s - 0x278361, e + 1231, NULL);

  char* our_code = s + 0x1111;
  char* not_our_code = s - 0x5555;
  char* not_our_code2 = e + 0x5555;

  // Exception in our code, but we have SEH filter => no dump.
  api.SetSEH(have_seh);
  api.SetStack(on_stack);
  EXPECT_CALL(api, WriteDump(_)).Times(0);
  EXPECT_EQ(ExceptionContinueSearch,
            veh.Handler(&ExceptionInfo(STATUS_ACCESS_VIOLATION, our_code)));
  testing::Mock::VerifyAndClearExpectations(&api);

  // RPC_E_DISCONNECTED (0x80010108) is "The object invoked has disconnected
  // from its clients", shall not be caught since it's a warning only.
  EXPECT_CALL(api, WriteDump(_)).Times(0);
  EXPECT_CALL(api, RtlpGetExceptionList()).Times(0);
  EXPECT_EQ(ExceptionContinueSearch,
      veh.Handler(&ExceptionInfo(RPC_E_DISCONNECTED, our_code)));
  testing::Mock::VerifyAndClearExpectations(&api);

  // Exception, not in our code, we do not have SEH and we are not in stack.
  api.SetSEH(no_seh);
  api.SetStack(not_on_stack);
  EXPECT_CALL(api, WriteDump(_)).Times(0);
  EXPECT_EQ(ExceptionContinueSearch,
      veh.Handler(&ExceptionInfo(STATUS_INTEGER_DIVIDE_BY_ZERO, not_our_code)));
  testing::Mock::VerifyAndClearExpectations(&api);

  // Exception, not in our code, no SEH, but we are on the stack.
  api.SetSEH(no_seh);
  api.SetStack(on_stack);
  EXPECT_CALL(api, WriteDump(_)).Times(1);
  EXPECT_EQ(ExceptionContinueSearch,
      veh.Handler(&ExceptionInfo(STATUS_INTEGER_DIVIDE_BY_ZERO,
                                 not_our_code2)));
  testing::Mock::VerifyAndClearExpectations(&api);

  // Exception, in our code, no SEH, not on stack (assume FPO screwed us)
  api.SetSEH(no_seh);
  api.SetStack(not_on_stack);
  EXPECT_CALL(api, WriteDump(_)).Times(1);
  EXPECT_EQ(ExceptionContinueSearch,
      veh.Handler(&ExceptionInfo(STATUS_PRIVILEGED_INSTRUCTION, our_code)));
  testing::Mock::VerifyAndClearExpectations(&api);

  // Exception, in IsBadStringPtrA, we are on the stack.
  char* is_bad_ptr = reinterpret_cast<char*>(GetProcAddress(
      GetModuleHandleA("kernel32.dll"), "IsBadStringPtrA"));
  SEHChain kernel32_seh(is_bad_ptr, is_bad_ptr + 0x100, NULL);
  api.SetSEH(kernel32_seh);
  api.SetStack(on_stack);
  EXPECT_CALL(api, WriteDump(_)).Times(0);
  EXPECT_EQ(ExceptionContinueSearch,
      veh.Handler(&ExceptionInfo(STATUS_ACCESS_VIOLATION, is_bad_ptr)));
  testing::Mock::VerifyAndClearExpectations(&api);

  // Exception, in IsBadStringPtrA, we are not on the stack.
  api.SetSEH(kernel32_seh);
  api.SetStack(not_on_stack);
  EXPECT_CALL(api, WriteDump(_)).Times(0);
  EXPECT_EQ(ExceptionContinueSearch,
      veh.Handler(&ExceptionInfo(STATUS_ACCESS_VIOLATION, is_bad_ptr)));
  testing::Mock::VerifyAndClearExpectations(&api);

  // Exception in a loading module, we are on the stack.
  // Make sure we don't report it.
  api.SetSEH(no_seh);
  api.SetStack(on_stack);
  EXPECT_CALL(api, WriteDump(_)).Times(0);

  g_mock_veh = &veh;
  void* id = ::AddVectoredExceptionHandler(FALSE, VEH);

  scoped_ptr<base::Environment> env(base::Environment::Create());
  EXPECT_TRUE(env->SetVar(WideToUTF8(kCrashOnLoadMode).c_str(), "1"));
  long exceptions_seen = veh.get_exceptions_seen();
  HMODULE module = ::LoadLibrary(kCrashDllName);
  EXPECT_EQ(NULL, module);

  testing::Mock::VerifyAndClearExpectations(&api);
  EXPECT_EQ(exceptions_seen + 1, veh.get_exceptions_seen());
  EXPECT_TRUE(env->UnSetVar(WideToUTF8(kCrashOnLoadMode).c_str()));

  // Exception in an unloading module, we are on the stack/
  // Make sure we report it.
  EXPECT_TRUE(env->SetVar(WideToUTF8(kCrashOnUnloadMode).c_str(), "1"));

  module = ::LoadLibrary(kCrashDllName);

  api.SetSEH(no_seh);
  api.SetStack(on_stack);
  EXPECT_CALL(api, WriteDump(_)).Times(1);
  EXPECT_TRUE(module != NULL);
  exceptions_seen = veh.get_exceptions_seen();

  // Crash on unloading.
  ::FreeLibrary(module);

  EXPECT_EQ(exceptions_seen + 1, veh.get_exceptions_seen());
  testing::Mock::VerifyAndClearExpectations(&api);

  ::RemoveVectoredExceptionHandler(id);
  g_mock_veh = NULL;
}

MATCHER_P(ExceptionCodeIs, code, "") {
  return (arg->ExceptionRecord->ExceptionCode == code);
}

DECLSPEC_NOINLINE static void OverflowStack() {
  char tmp[1024 * 2048] = {0};
  GetSystemInfo(reinterpret_cast<SYSTEM_INFO*>(&tmp));
}

DWORD WINAPI CrashingThread(PVOID tmp) {
  __try {
    OverflowStack();
  } __except(EXCEPTION_EXECUTE_HANDLER) {

  }

  // This will cause STATUS_ACCESS_VIOLATION
  __try {
    OverflowStack();
  } __except(EXCEPTION_EXECUTE_HANDLER) {

  }

  return 0;
}

TEST(ChromeFrame, TrickyStackOverflow) {
  MockApi api;
  VectoredHandlerMock veh(&api);

  // Start address of our module.
  char* s = reinterpret_cast<char*>(0x30000000);
  char *e = s + 0x10000;
  api.SetModule(s, e);

  SEHChain no_seh(s - 0x1000, e + 0x7127, NULL);
  StackHelper on_stack(s - 0x11283, s - 0x278361, s + 0x9171, e + 1231, NULL);
  api.SetSEH(no_seh);
  api.SetStack(on_stack);

  EXPECT_CALL(api, WriteDump(ExceptionCodeIs(STATUS_STACK_OVERFLOW))).Times(1);

  g_mock_veh = &veh;
  void* id = ::AddVectoredExceptionHandler(FALSE, VEH);

  DWORD tid;
  HANDLE h = ::CreateThread(0, 0, CrashingThread, 0, 0, &tid);
  ::WaitForSingleObject(h, INFINITE);
  ::CloseHandle(h);

  EXPECT_EQ(2, veh.get_exceptions_seen());
  ::RemoveVectoredExceptionHandler(id);
  g_mock_veh = NULL;
}
