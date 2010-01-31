// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include "chrome_frame/crash_reporting/veh_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "chrome_frame/crash_reporting/vectored_handler-impl.h"

#pragma code_seg(push, ".m$_0")
static void ModuleStart() {}
#pragma code_seg(pop)

#pragma code_seg(push, ".m$_2")
static void Undetectable(DWORD code) {
  __try {
    ::RaiseException(code, 0, 0, NULL);
  } __except(EXCEPTION_EXECUTE_HANDLER) {

  }
};
#pragma code_seg(pop)

#pragma code_seg(push, ".m$_3")
static void UndetectableEnd() {}
#pragma code_seg(pop)

#pragma code_seg(push, ".m$_4")
static void CatchThis() {
  __try {
    ::RaiseException(STATUS_ACCESS_VIOLATION, 0, 0, NULL);
  } __except(EXCEPTION_EXECUTE_HANDLER) {

  }

  // this will be detected since we are on the stack!
  Undetectable(STATUS_ILLEGAL_INSTRUCTION);
}

#pragma code_seg(pop)

#pragma code_seg(push, ".m$_9")
static void ModuleEnd() {}
#pragma code_seg(pop)

using testing::_;
namespace {
MATCHER_P(ExceptionCodeIs, code, "") {
  return (arg->ExceptionRecord->ExceptionCode == code);
}

class MockApi : public Win32VEHTraits,
                public ModuleOfInterestWithExcludedRegion {
 public:
  MockApi() {
    Win32VEHTraits::InitializeIgnoredBlocks();
    ModuleOfInterestWithExcludedRegion::SetModule(&ModuleStart, &ModuleEnd);
    ModuleOfInterestWithExcludedRegion::SetExcludedRegion(&Undetectable,
        &UndetectableEnd);
  }

  MOCK_METHOD1(WriteDump, void(const EXCEPTION_POINTERS*));
  MOCK_METHOD0(RtlpGetExceptionList, const EXCEPTION_REGISTRATION_RECORD*());
};
};  // namespace

typedef VectoredHandlerT<MockApi> VectoredHandlerMock;

static VectoredHandlerMock* g_mock_veh = NULL;
static LONG WINAPI VEH(EXCEPTION_POINTERS* exptrs) {
  return g_mock_veh->Handler(exptrs);
}

TEST(ChromeFrame, ExceptionExcludedCode) {
  MockApi api;
  VectoredHandlerMock veh(&api);

  g_mock_veh = &veh;
  void* id = ::AddVectoredExceptionHandler(FALSE, VEH);

  EXPECT_CALL(api, RtlpGetExceptionList())
      .WillRepeatedly(testing::Return(EXCEPTION_CHAIN_END));

  testing::Sequence s;

  EXPECT_CALL(api, WriteDump(ExceptionCodeIs(STATUS_ACCESS_VIOLATION)))
    .Times(1);

  EXPECT_CALL(api, WriteDump(ExceptionCodeIs(STATUS_ILLEGAL_INSTRUCTION)))
      .Times(1);

  CatchThis();
  EXPECT_EQ(2, veh.get_exceptions_seen());

  // Not detected since we are not on the stack.
  Undetectable(STATUS_INTEGER_DIVIDE_BY_ZERO);
  EXPECT_EQ(3, veh.get_exceptions_seen());

  ::RemoveVectoredExceptionHandler(id);
  g_mock_veh = NULL;
}


