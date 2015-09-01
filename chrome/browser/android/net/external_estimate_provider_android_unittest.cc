// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/net/external_estimate_provider_android.h"

#include <stdint.h>

#include "base/time/time.h"
#include "net/base/network_quality_estimator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Tests if the |ExternalEstimateProviderAndroid| APIs return false without the
// downstream implementation.
TEST(ExternalEstimateProviderAndroidTest, BasicsTest) {
  chrome::android::ExternalEstimateProviderAndroid external_estimate_provider;

  base::TimeDelta rtt;
  EXPECT_FALSE(external_estimate_provider.GetRTT(&rtt));

  int32_t kbps;
  EXPECT_FALSE(external_estimate_provider.GetDownstreamThroughputKbps(&kbps));
  EXPECT_FALSE(external_estimate_provider.GetUpstreamThroughputKbps(&kbps));

  base::TimeDelta time_since_last_update;
  EXPECT_FALSE(external_estimate_provider.GetTimeSinceLastUpdate(
      &time_since_last_update));
}

class TestNetworkQualityEstimator : public net::NetworkQualityEstimator {
 public:
  TestNetworkQualityEstimator(
      scoped_ptr<chrome::android::ExternalEstimateProviderAndroid>
          external_estimate_provider,
      const std::map<std::string, std::string>& variation_params)
      : NetworkQualityEstimator(external_estimate_provider.Pass(),
                                variation_params),
        notified_(false) {}

  ~TestNetworkQualityEstimator() override {}

  void OnUpdatedEstimateAvailable() override { notified_ = true; }

  bool IsNotified() const { return notified_; }

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
};

// Tests if the |ExternalEstimateProviderAndroid| notifies
// |NetworkQualityEstimator|.
TEST(ExternalEstimateProviderAndroidTest, DelegateTest) {
  scoped_ptr<TestExternalEstimateProviderAndroid> external_estimate_provider;
  external_estimate_provider.reset(new TestExternalEstimateProviderAndroid());

  TestExternalEstimateProviderAndroid* ptr = external_estimate_provider.get();
  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator network_quality_estimator(
      external_estimate_provider.Pass(), variation_params);
  ptr->NotifyUpdatedEstimateAvailable();
  DCHECK(network_quality_estimator.IsNotified());
}

}  // namespace
