// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service_factory.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service_test_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/nqe/effective_connection_type.h"
#include "testing/gtest/include/gtest/gtest.h"

class Profile;

namespace {

using UINetworkQualityEstimatorServiceBrowserTest = InProcessBrowserTest;

}  // namespace

IN_PROC_BROWSER_TEST_F(UINetworkQualityEstimatorServiceBrowserTest,
                       VerifyNQEState) {
  // Verifies that NQE notifying EffectiveConnectionTypeObservers causes the
  // UINetworkQualityEstimatorService to receive an updated
  // EffectiveConnectionType.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  UINetworkQualityEstimatorService* nqe_service =
      UINetworkQualityEstimatorServiceFactory::GetForProfile(profile);

  nqe_test_util::OverrideEffectiveConnectionTypeAndWait(
      net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
            nqe_service->GetEffectiveConnectionType());

  nqe_test_util::OverrideEffectiveConnectionTypeAndWait(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            nqe_service->GetEffectiveConnectionType());
}
