// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for the Crash Reporter.

#include "ceee/ie/common/crash_reporter.h"

#include "base/logging.h"
#include "base/win/pe_image.h"
#include "ceee/ie/common/ceee_module_util.h"

#include "ceee/testing/utils/mock_window_utils.h"
#include "ceee/testing/utils/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::Return;
using testing::StrictMock;

// A helper for just extracting the default exception filter.
static LPTOP_LEVEL_EXCEPTION_FILTER GetUnhandledExceptionFilter() {
  LPTOP_LEVEL_EXCEPTION_FILTER old_exh = SetUnhandledExceptionFilter(NULL);
  EXPECT_EQ(NULL, SetUnhandledExceptionFilter(old_exh));
  return old_exh;
}

// A helper for just extracting the default invalid parameter handler.
static _invalid_parameter_handler GetInvalidParameterHandler() {
  _invalid_parameter_handler old_iph = _set_invalid_parameter_handler(NULL);
  EXPECT_EQ(NULL, _set_invalid_parameter_handler(old_iph));
  return old_iph;
}

// A helper for just extracting the default purecall handler.
static _purecall_handler GetPureCallHandler() {
  _purecall_handler old_pch = _set_purecall_handler(NULL);
  EXPECT_EQ(NULL, _set_purecall_handler(old_pch));
  return old_pch;
}

MOCK_STATIC_CLASS_BEGIN(MockCeeeModuleUtil)
  MOCK_STATIC_INIT_BEGIN(MockCeeeModuleUtil)
    MOCK_STATIC_INIT2(ceee_module_util::GetCollectStatsConsent,
                      GetCollectStatsConsent);
  MOCK_STATIC_INIT_END()
  MOCK_STATIC0(bool, , GetCollectStatsConsent);
MOCK_STATIC_CLASS_END(MockCeeeModuleUtil)

// Test that basic exception handling from Breakpad works, by ensuring that
// calling CrashReporter functions correctly replaces default handlers.
TEST(CrashReporterTest, ExceptionHandling) {
  base::win::PEImage my_image(reinterpret_cast<HMODULE>(&__ImageBase));

  // Take the default handlers out and do a quick sanity check. It is needed
  // later to ensure it gets replaced by breakpad.
  LPTOP_LEVEL_EXCEPTION_FILTER orig_exh = GetUnhandledExceptionFilter();
  EXPECT_TRUE(orig_exh != NULL);

  _invalid_parameter_handler orig_iph = GetInvalidParameterHandler();
  EXPECT_EQ(NULL, orig_iph);

  _purecall_handler orig_pch = GetPureCallHandler();
  EXPECT_EQ(NULL, orig_pch);

  // Initialize and ensure that a new handler has replaced the original
  // handler, and that the new handler is within this image/from breakpad.
  CrashReporter crash_reporter(L"unittest");

  StrictMock<MockCeeeModuleUtil> mock;
  EXPECT_CALL(mock, GetCollectStatsConsent())
        .Times(1)
        .WillOnce(Return(false));

  crash_reporter.InitializeCrashReporting(false);
  EXPECT_EQ(orig_exh, GetUnhandledExceptionFilter());
  crash_reporter.ShutdownCrashReporting();

  EXPECT_CALL(mock, GetCollectStatsConsent())
        .Times(1)
        .WillOnce(Return(true));
  crash_reporter.InitializeCrashReporting(false);

  LPTOP_LEVEL_EXCEPTION_FILTER new_exh = GetUnhandledExceptionFilter();
  EXPECT_TRUE(my_image.GetImageSectionFromAddr((PVOID)new_exh) != NULL);
  EXPECT_TRUE(orig_exh != new_exh);

  _invalid_parameter_handler new_iph = GetInvalidParameterHandler();
  EXPECT_TRUE(my_image.GetImageSectionFromAddr((PVOID)new_iph) != NULL);
  EXPECT_TRUE(orig_iph != new_iph);

  _purecall_handler new_pch = GetPureCallHandler();
  EXPECT_TRUE(my_image.GetImageSectionFromAddr((PVOID)new_pch) != NULL);
  EXPECT_TRUE(orig_pch != new_pch);

  // Shut down, and ensure that the original exception handler is replaced
  // by breakpad.
  crash_reporter.ShutdownCrashReporting();

  LPTOP_LEVEL_EXCEPTION_FILTER final_exh = GetUnhandledExceptionFilter();
  EXPECT_EQ(orig_exh, final_exh);

  _invalid_parameter_handler final_iph = GetInvalidParameterHandler();
  EXPECT_EQ(orig_iph, final_iph);

  _purecall_handler final_pch = GetPureCallHandler();
  EXPECT_EQ(orig_pch, final_pch);
}

}  // namespace
