// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"

#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeMetricsServiceAccessorTest : public testing::Test {
 public:
  ChromeMetricsServiceAccessorTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {
  }

  PrefService* GetLocalState() {
    return testing_local_state_.Get();
  }

 private:
  ScopedTestingLocalState testing_local_state_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMetricsServiceAccessorTest);
};

TEST_F(ChromeMetricsServiceAccessorTest, MetricsReportingEnabled) {
#if !defined(OS_CHROMEOS)
  GetLocalState()->SetBoolean(prefs::kMetricsReportingEnabled, false);
  EXPECT_FALSE(ChromeMetricsServiceAccessor::IsMetricsReportingEnabled());
  GetLocalState()->SetBoolean(prefs::kMetricsReportingEnabled, true);
  EXPECT_TRUE(ChromeMetricsServiceAccessor::IsMetricsReportingEnabled());
  GetLocalState()->ClearPref(prefs::kMetricsReportingEnabled);
  EXPECT_FALSE(ChromeMetricsServiceAccessor::IsMetricsReportingEnabled());
#else
  // ChromeOS does not register prefs::kMetricsReportingEnabled and uses
  // device settings for metrics reporting.
  EXPECT_FALSE(ChromeMetricsServiceAccessor::IsMetricsReportingEnabled());
#endif
}

TEST_F(ChromeMetricsServiceAccessorTest, CrashReportingEnabled) {
#if defined(GOOGLE_CHROME_BUILD)
// ChromeOS has different device settings for crash reporting.
#if !defined(OS_CHROMEOS)
#if defined(OS_ANDROID)
  const char* crash_pref = prefs::kCrashReportingEnabled;
#else
  const char* crash_pref = prefs::kMetricsReportingEnabled;
#endif
  GetLocalState()->SetBoolean(crash_pref, false);
  EXPECT_FALSE(ChromeMetricsServiceAccessor::IsCrashReportingEnabled());
  GetLocalState()->SetBoolean(crash_pref, true);
  EXPECT_TRUE(ChromeMetricsServiceAccessor::IsCrashReportingEnabled());
  GetLocalState()->ClearPref(crash_pref);
  EXPECT_FALSE(ChromeMetricsServiceAccessor::IsCrashReportingEnabled());
#endif  // !defined(OS_CHROMEOS)
#else  // defined(GOOGLE_CHROME_BUILD)
  // Chromium branded browsers never have crash reporting enabled.
  EXPECT_FALSE(ChromeMetricsServiceAccessor::IsCrashReportingEnabled());
#endif  // defined(GOOGLE_CHROME_BUILD)
}
