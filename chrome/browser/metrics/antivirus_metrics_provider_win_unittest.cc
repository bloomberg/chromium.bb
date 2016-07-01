// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/antivirus_metrics_provider_win.h"

#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "base/win/windows_version.h"
#include "components/variations/metrics_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Helper function to toggle whether the ReportFullAVProductDetails feature is
// enabled or not.
void SetFullNamesFeatureEnabled(bool enabled) {
  base::FeatureList::ClearInstanceForTesting();
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  if (enabled) {
    feature_list->InitializeFromCommandLine(
        AntiVirusMetricsProvider::kReportNamesFeature.name, std::string());
  } else {
    feature_list->InitializeFromCommandLine(
        std::string(), AntiVirusMetricsProvider::kReportNamesFeature.name);
  }
  base::FeatureList::SetInstance(std::move(feature_list));
}

void VerifySystemProfileData(const metrics::SystemProfileProto& system_profile,
                             bool expect_unhashed_value) {
  const char kWindowsDefender[] = "Windows Defender";

  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    bool defender_found = false;
    for (const auto& av : system_profile.antivirus_product()) {
      if (av.product_name_hash() == metrics::HashName(kWindowsDefender)) {
        defender_found = true;
        if (expect_unhashed_value) {
          EXPECT_TRUE(av.has_product_name());
          EXPECT_EQ(kWindowsDefender, av.product_name());
        } else {
          EXPECT_FALSE(av.has_product_name());
        }
        break;
      }
    }
    EXPECT_TRUE(defender_found);
  }
}

}  // namespace

class AntiVirusMetricsProviderTest : public ::testing::TestWithParam<bool> {
 public:
  AntiVirusMetricsProviderTest()
      : got_results_(false),
        expect_unhashed_value_(GetParam()),
        provider_(new AntiVirusMetricsProvider(
            content::BrowserThread::GetMessageLoopProxyForThread(
                content::BrowserThread::FILE))),
        thread_bundle_(content::TestBrowserThreadBundle::REAL_FILE_THREAD),
        weak_ptr_factory_(this) {}

  void GetMetricsCallback() {
    // Check that the callback runs on the main loop.
    ASSERT_TRUE(thread_checker_.CalledOnValidThread());
    ASSERT_TRUE(run_loop_.running());

    run_loop_.QuitWhenIdle();

    got_results_ = true;

    metrics::SystemProfileProto system_profile;
    provider_->ProvideSystemProfileMetrics(&system_profile);

    VerifySystemProfileData(system_profile, expect_unhashed_value_);
    // This looks weird, but it's to make sure that reading the data out of the
    // AntiVirusMetricsProvider does not invalidate it, as the class should be
    // resilient to this.
    system_profile.Clear();
    provider_->ProvideSystemProfileMetrics(&system_profile);
    VerifySystemProfileData(system_profile, expect_unhashed_value_);
  }

  bool got_results_;
  bool expect_unhashed_value_;
  std::unique_ptr<AntiVirusMetricsProvider> provider_;
  content::TestBrowserThreadBundle thread_bundle_;
  base::RunLoop run_loop_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<AntiVirusMetricsProviderTest> weak_ptr_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AntiVirusMetricsProviderTest);
};

TEST_P(AntiVirusMetricsProviderTest, GetMetricsFullName) {
  ASSERT_TRUE(thread_checker_.CalledOnValidThread());
  base::HistogramTester histograms;
  SetFullNamesFeatureEnabled(expect_unhashed_value_);
  // Make sure the I/O is happening on the FILE thread by disallowing it on
  // the main thread.
  bool previous_value = base::ThreadRestrictions::SetIOAllowed(false);
  provider_->GetAntiVirusMetrics(
      base::Bind(&AntiVirusMetricsProviderTest::GetMetricsCallback,
                 weak_ptr_factory_.GetWeakPtr()));
  content::RunThisRunLoop(&run_loop_);
  EXPECT_TRUE(got_results_);
  base::ThreadRestrictions::SetIOAllowed(previous_value);

  AntiVirusMetricsProvider::ResultCode expected_result =
      AntiVirusMetricsProvider::RESULT_SUCCESS;
  if (base::win::OSInfo::GetInstance()->version_type() ==
      base::win::SUITE_SERVER)
    expected_result = AntiVirusMetricsProvider::RESULT_WSC_NOT_AVAILABLE;
  histograms.ExpectUniqueSample("UMA.AntiVirusMetricsProvider.Result",
                                expected_result, 1);
}

INSTANTIATE_TEST_CASE_P(, AntiVirusMetricsProviderTest, ::testing::Bool());
