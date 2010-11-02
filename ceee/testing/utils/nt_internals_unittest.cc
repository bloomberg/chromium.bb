// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Test the functions defined in nt_internals.h.
#include "ceee/testing/utils/nt_internals.h"
#include <iostream>
#include "gtest/gtest.h"
#include "ceee/testing/utils/gflag_utils.h"

#define STATUS_SUCCESS ((NTSTATUS)0)

namespace nt_internals {

TEST(NtInternalsTest, GetThreadTIB) {
  ASSERT_TRUE(NULL != GetThreadTIB(::GetCurrentThread()));
}

TEST(NtInternalsTest, NtQueryInformationThread) {
  ASSERT_TRUE(NULL != NtQueryInformationThread);
  THREAD_BASIC_INFO info = {};
  ULONG ret_len = 0;
  NTSTATUS status = NtQueryInformationThread(::GetCurrentThread(),
                ThreadBasicInformation, &info, sizeof(info), &ret_len);
  EXPECT_EQ(sizeof(info), ret_len);
  EXPECT_TRUE(NULL != info.TebBaseAddress);
  EXPECT_EQ(reinterpret_cast<DWORD>(info.ClientId.UniqueThread),
                                    ::GetCurrentThreadId());
  EXPECT_EQ(reinterpret_cast<DWORD>(info.ClientId.UniqueProcess),
                                    ::GetCurrentProcessId());
}

TEST(NtInternalsTest, RtlCreateQueryDebugBuffer) {
  ASSERT_TRUE(NULL != RtlCreateQueryDebugBuffer);
  ASSERT_TRUE(NULL != RtlDestroyQueryDebugBuffer);

  PRTL_DEBUG_INFORMATION debug_info = RtlCreateQueryDebugBuffer(0, 0);
  ASSERT_TRUE(NULL != debug_info);
  RtlDestroyQueryDebugBuffer(debug_info);
}

TEST(NtInternalsTest, RtlQueryProcessDebugInformation) {
  ASSERT_TRUE(NULL != RtlCreateQueryDebugBuffer);
  ASSERT_TRUE(NULL != RtlDestroyQueryDebugBuffer);
  ASSERT_TRUE(NULL != RtlQueryProcessDebugInformation);
  ASSERT_TRUE(NULL != RtlGetNtGlobalFlags);

  PRTL_DEBUG_INFORMATION debug_info = RtlCreateQueryDebugBuffer(0, 0);
  ASSERT_TRUE(NULL != debug_info);

  NTSTATUS status =
      RtlQueryProcessDebugInformation(::GetCurrentProcessId(),
                                      PDI_MODULES | PDI_BACKTRACE,
                                      debug_info);
  EXPECT_EQ(STATUS_SUCCESS, status);

  ASSERT_TRUE(NULL != debug_info->Modules);
  EXPECT_LT(0U, debug_info->Modules->NumberOfModules);

  // The global flag is usually set by our main function, but when
  // we e.g. run under app verifier it won't. For some reason we
  // get stack backtraces under app verifier even when the backtrace
  // flag is not set.
  // ASSERT_TRUE(FLG_USER_STACK_TRACE_DB & RtlGetNtGlobalFlags());

  ASSERT_TRUE(NULL != debug_info->BackTraces);
  EXPECT_LT(0U, debug_info->BackTraces->NumberOfBackTraces);

  status = RtlDestroyQueryDebugBuffer(debug_info);
  EXPECT_EQ(STATUS_SUCCESS, status);
}

TEST(NtInternalsTest, RtlQueryProcessBackTraceInformation) {
  ASSERT_TRUE(NULL != RtlQueryProcessBackTraceInformation);
  ASSERT_TRUE(NULL != RtlGetNtGlobalFlags);

  // Size gleaned from buffers returned from RtlCreateQueryDebugBuffer.
  static char buf[0x00400000];
  PRTL_DEBUG_INFORMATION info = reinterpret_cast<PRTL_DEBUG_INFORMATION>(buf);

  memset(buf, 0, sizeof(buf));
  info->OffsetFree = sizeof(RTL_DEBUG_INFORMATION);
  info->ViewSize = sizeof(buf);
  info->CommitSize = sizeof(buf);
  NTSTATUS status = RtlQueryProcessBackTraceInformation(info);
  ASSERT_EQ(STATUS_SUCCESS, status);

  // The global flag is usually set by our main function, but when
  // we e.g. run under app verifier it won't. For some reason we
  // get stack backtraces under app verifier even when the backtrace
  // flag is not set.
  // ASSERT_TRUE(FLG_USER_STACK_TRACE_DB & RtlGetNtGlobalFlags());

  ASSERT_TRUE(info->BackTraces != NULL);
  ASSERT_LT(0UL, info->BackTraces->NumberOfBackTraces);
}

TEST(NtInternalsTest, RtlGetNtGlobalFlags) {
  // No crash...
  RtlGetNtGlobalFlags();
}

}  // namespace nt_internals
