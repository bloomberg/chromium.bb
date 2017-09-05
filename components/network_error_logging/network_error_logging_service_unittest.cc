// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "components/network_error_logging/network_error_logging_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class NetworkErrorLoggingServiceTest : public ::testing::Test {
 protected:
  NetworkErrorLoggingServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kNetworkErrorLogging);
    service_ = network_error_logging::NetworkErrorLoggingService::Create();
  }

  network_error_logging::NetworkErrorLoggingService* service() {
    return service_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<network_error_logging::NetworkErrorLoggingService> service_;
};

TEST_F(NetworkErrorLoggingServiceTest, FeatureDisabled) {
  // N.B. This test does not actually use the test fixture.

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kNetworkErrorLogging);

  auto service = network_error_logging::NetworkErrorLoggingService::Create();
  EXPECT_FALSE(service);
}

TEST_F(NetworkErrorLoggingServiceTest, FeatureEnabled) {
  EXPECT_TRUE(service());
}

}  // namespace
