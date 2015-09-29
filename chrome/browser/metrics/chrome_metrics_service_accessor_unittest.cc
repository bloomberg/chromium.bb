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
#if defined(GOOGLE_CHROME_BUILD)
#if !defined(OS_CHROMEOS)
#if defined(OS_ANDROID)
  const char* pref = prefs::kCrashReportingEnabled;
#else
  const char* pref = metrics::prefs::kMetricsReportingEnabled;
#endif  // defined(OS_ANDROID)
  GetLocalState()->SetDefaultPrefValue(pref, new base::FundamentalValue(false));

  GetLocalState()->SetBoolean(pref, false);
  EXPECT_FALSE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
  GetLocalState()->SetBoolean(pref, true);
  EXPECT_TRUE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
  GetLocalState()->ClearPref(pref);
  EXPECT_FALSE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
#endif  // !defined(OS_CHROMEOS)
#else
  // Metrics Reporting is never enabled when GOOGLE_CHROME_BUILD is undefined.
  EXPECT_FALSE(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
#endif
}
