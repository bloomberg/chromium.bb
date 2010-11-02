// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IE toolband module reporting unit tests.
#include "ceee/ie/plugin/toolband/toolband_module_reporting.h"

#include "ceee/ie/common/ceee_module_util.h"
#include "ceee/testing/utils/mock_static.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::_;
using testing::Return;
using testing::StrictMock;

MOCK_STATIC_CLASS_BEGIN(MockToolbandModuleReporting)
  MOCK_STATIC_INIT_BEGIN(MockToolbandModuleReporting)
    MOCK_STATIC_INIT2(ceee_module_util::GetCollectStatsConsent,
                      GetCollectStatsConsent);
    MOCK_STATIC_INIT(InitializeVectoredCrashReporting);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC0(bool, , GetCollectStatsConsent);
  MOCK_STATIC4(bool, , InitializeVectoredCrashReporting, bool,
      const wchar_t*,
      const std::wstring&,
      google_breakpad::CustomClientInfo*);
MOCK_STATIC_CLASS_END(MockToolbandModuleReporting)

TEST(ToolbandModuleReportingTest, InitializeCrashReportingWithoutConsent) {
  StrictMock<MockToolbandModuleReporting> mock;

  EXPECT_CALL(mock, GetCollectStatsConsent())
        .Times(1)
        .WillOnce(Return(false));

  EXPECT_CALL(mock, InitializeVectoredCrashReporting(_, _, _, _))
        .Times(0);

  InitializeCrashReporting();
}

TEST(ToolbandModuleReportingTest, InitializeCrashReportingWithConsent) {
  StrictMock<MockToolbandModuleReporting> mock;

  EXPECT_CALL(mock, GetCollectStatsConsent())
        .Times(1)
        .WillOnce(Return(true));

  EXPECT_CALL(mock, InitializeVectoredCrashReporting(_, _, _, _))
        .Times(1)
        .WillOnce(Return(true));

  InitializeCrashReporting();
}

}  // namespace
