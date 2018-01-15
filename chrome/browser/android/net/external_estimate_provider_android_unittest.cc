// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/net/external_estimate_provider_android.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/at_exit.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/log/test_net_log.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Check that |ExternalEstimateProviderAndroid|  does not crash without the
// downstream implementation.
TEST(ExternalEstimateProviderAndroidTest, BasicsTest) {
  base::ShadowingAtExitManager at_exit_manager;
  chrome::android::ExternalEstimateProviderAndroid external_estimate_provider;
  external_estimate_provider.Update();
}

class TestNetworkQualityEstimator : public net::NetworkQualityEstimator {
 public:
  TestNetworkQualityEstimator(
      std::unique_ptr<chrome::android::ExternalEstimateProviderAndroid>
          external_estimate_provider,
      const std::map<std::string, std::string>& variation_params,
      net::NetLog* net_log)
      : NetworkQualityEstimator(
            std::move(external_estimate_provider),
            std::make_unique<net::NetworkQualityEstimatorParams>(
                variation_params),
            net_log),
        notified_(false) {}

  ~TestNetworkQualityEstimator() override {}

  void OnUpdatedEstimateAvailable(const base::TimeDelta& rtt,
                                  int32_t downstream_throughput_kbps) override {
    EXPECT_EQ(base::TimeDelta::FromSeconds(1), rtt);
    EXPECT_EQ(200, downstream_throughput_kbps);
    notified_ = true;
    net::NetworkQualityEstimator::OnUpdatedEstimateAvailable(
        rtt, downstream_throughput_kbps);
  }

  bool notified() const { return notified_; }

 private:
  bool notified_;
};

class TestExternalEstimateProviderAndroid
    : public chrome::android::ExternalEstimateProviderAndroid {
 public:
  TestExternalEstimateProviderAndroid()
      : chrome::android::ExternalEstimateProviderAndroid() {}
  ~TestExternalEstimateProviderAndroid() override {}
  using ExternalEstimateProviderAndroid::NotifyUpdatedEstimateAvailable;

 private:
  base::TimeDelta GetRTT() const override {
    return base::TimeDelta::FromSeconds(1);
  }

  int32_t GetDownstreamThroughputKbps() const override { return 200; }
};

// Tests if the |ExternalEstimateProviderAndroid| notifies
// |NetworkQualityEstimator|.
TEST(ExternalEstimateProviderAndroidTest, DelegateTest) {
  content::TestBrowserThreadBundle thread_bundle(
      content::TestBrowserThreadBundle::IO_MAINLOOP);

  base::ShadowingAtExitManager at_exit_manager;
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestExternalEstimateProviderAndroid>
      external_estimate_provider;
  external_estimate_provider.reset(new TestExternalEstimateProviderAndroid());

  TestExternalEstimateProviderAndroid* ptr = external_estimate_provider.get();
  net::TestNetLog net_log;
  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator network_quality_estimator(
      std::move(external_estimate_provider), variation_params, &net_log);
  ptr->NotifyUpdatedEstimateAvailable();
  EXPECT_TRUE(network_quality_estimator.notified());

  histogram_tester.ExpectTotalCount("NQE.ExternalEstimateProviderStatus", 4);

  // EXTERNAL_ESTIMATE_PROVIDER_STATUS_AVAILABLE
  histogram_tester.ExpectBucketCount("NQE.ExternalEstimateProviderStatus", 1,
                                     1);

  // EXTERNAL_ESTIMATE_PROVIDER_STATUS_CALLBACK
  histogram_tester.ExpectBucketCount("NQE.ExternalEstimateProviderStatus", 4,
                                     1);
}

}  // namespace
