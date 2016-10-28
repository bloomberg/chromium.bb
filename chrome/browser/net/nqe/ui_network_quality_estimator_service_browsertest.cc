// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/bind.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service_factory.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service_test_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "net/base/network_change_notifier.h"
#include "net/nqe/cached_network_quality.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_id.h"
#include "net/nqe/network_quality_estimator.h"
#include "testing/gtest/include/gtest/gtest.h"

class Profile;

namespace {

class TestEffectiveConnectionTypeObserver
    : public net::NetworkQualityEstimator::EffectiveConnectionTypeObserver {
 public:
  TestEffectiveConnectionTypeObserver() {}
  ~TestEffectiveConnectionTypeObserver() override {}

  // net::NetworkQualityEstimator::EffectiveConnectionTypeObserver
  // implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override {
    effective_connection_type_ = type;
  }

  // The most recently set EffectiveConnectionType.
  net::EffectiveConnectionType effective_connection_type() const {
    return effective_connection_type_;
  }

 private:
  net::EffectiveConnectionType effective_connection_type_;
};

class UINetworkQualityEstimatorServiceBrowserTest
    : public InProcessBrowserTest {
 public:
  UINetworkQualityEstimatorServiceBrowserTest() {}

  // Enables persistent caching if |persistent_caching_enabled| is true.
  // Verifies that the network quality prefs are written correctly, and that
  // they are written only if the persistent caching was enabled.
  void VerifyWritingReadingPrefs(bool persistent_caching_enabled) {
    if (persistent_caching_enabled) {
      std::map<std::string, std::string> variation_params;
      variation_params["persistent_caching_enabled"] = "true";

      variations::AssociateVariationParams("NetworkQualityEstimator", "Enabled",
                                           variation_params);
      base::FieldTrialList::CreateFieldTrial("NetworkQualityEstimator",
                                             "Enabled");
    }

    // Verifies that NQE notifying EffectiveConnectionTypeObservers causes the
    // UINetworkQualityEstimatorService to receive an updated
    // EffectiveConnectionType.
    Profile* profile = ProfileManager::GetActiveUserProfile();

    bool network_id_available = true;
    if (net::NetworkChangeNotifier::GetConnectionType() ==
            net::NetworkChangeNotifier::CONNECTION_UNKNOWN ||
        net::NetworkChangeNotifier::GetConnectionType() ==
            net::NetworkChangeNotifier::CONNECTION_NONE ||
        net::NetworkChangeNotifier::GetConnectionType() ==
            net::NetworkChangeNotifier::CONNECTION_BLUETOOTH) {
      network_id_available = false;
    }

    UINetworkQualityEstimatorService* nqe_service =
        UINetworkQualityEstimatorServiceFactory::GetForProfile(profile);
    ASSERT_NE(nullptr, nqe_service);
    // NetworkQualityEstimator must be notified of the read prefs at startup.
    EXPECT_FALSE(histogram_tester_.GetAllSamples("NQE.Prefs.ReadSize").empty());

    {
      base::HistogramTester histogram_tester;
      nqe_test_util::OverrideEffectiveConnectionTypeAndWait(
          net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
      EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
                nqe_service->GetEffectiveConnectionType());

      // Prefs are written only if the network id was available, and persistent
      // caching was enabled.
      EXPECT_NE(network_id_available && persistent_caching_enabled,
                histogram_tester.GetAllSamples("NQE.Prefs.WriteCount").empty());
      histogram_tester.ExpectTotalCount("NQE.Prefs.ReadCount", 0);

      // NetworkQualityEstimator should not be notified of change in prefs.
      histogram_tester.ExpectTotalCount("NQE.Prefs.ReadSize", 0);
    }

    {
      base::HistogramTester histogram_tester;
      nqe_test_util::OverrideEffectiveConnectionTypeAndWait(
          net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

      EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
                nqe_service->GetEffectiveConnectionType());

      // Prefs are written only if the network id was available, and persistent
      // caching was enabled.
      EXPECT_NE(network_id_available && persistent_caching_enabled,
                histogram_tester.GetAllSamples("NQE.Prefs.WriteCount").empty());
      histogram_tester.ExpectTotalCount("NQE.Prefs.ReadCount", 0);

      // NetworkQualityEstimator should not be notified of change in prefs.
      histogram_tester.ExpectTotalCount("NQE.Prefs.ReadSize", 0);
    }

    // Verify the contents of the prefs by reading them again.
    std::map<net::nqe::internal::NetworkID,
             net::nqe::internal::CachedNetworkQuality>
        read_prefs = nqe_service->ForceReadPrefsForTesting();
    EXPECT_EQ(network_id_available && persistent_caching_enabled ? 1u : 0u,
              read_prefs.size());
    if (network_id_available && persistent_caching_enabled) {
      // Verify that the cached network quality was written correctly.
      EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
                read_prefs.begin()->second.effective_connection_type());
      if (net::NetworkChangeNotifier::GetConnectionType() ==
          net::NetworkChangeNotifier::CONNECTION_ETHERNET) {
        // Verify that the network ID was written correctly.
        net::nqe::internal::NetworkID ethernet_network_id(
            net::NetworkChangeNotifier::CONNECTION_ETHERNET, std::string());
        EXPECT_EQ(ethernet_network_id, read_prefs.begin()->first);
      }
    }
  }

 private:
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(UINetworkQualityEstimatorServiceBrowserTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(UINetworkQualityEstimatorServiceBrowserTest,
                       VerifyNQEState) {
  // Verifies that NQE notifying EffectiveConnectionTypeObservers causes the
  // UINetworkQualityEstimatorService to receive an updated
  // EffectiveConnectionType.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  UINetworkQualityEstimatorService* nqe_service =
      UINetworkQualityEstimatorServiceFactory::GetForProfile(profile);
  TestEffectiveConnectionTypeObserver nqe_observer;
  nqe_service->AddEffectiveConnectionTypeObserver(&nqe_observer);

  nqe_test_util::OverrideEffectiveConnectionTypeAndWait(
      net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
            nqe_service->GetEffectiveConnectionType());
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
            nqe_observer.effective_connection_type());

  nqe_test_util::OverrideEffectiveConnectionTypeAndWait(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            nqe_service->GetEffectiveConnectionType());
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            nqe_observer.effective_connection_type());

  nqe_service->RemoveEffectiveConnectionTypeObserver(&nqe_observer);

  nqe_test_util::OverrideEffectiveConnectionTypeAndWait(
      net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
            nqe_service->GetEffectiveConnectionType());
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            nqe_observer.effective_connection_type());
}

// Verify that prefs are not writen when writing of the prefs is not enabled
// via field trial.
IN_PROC_BROWSER_TEST_F(UINetworkQualityEstimatorServiceBrowserTest,
                       WritingToPrefsDisabled) {
  VerifyWritingReadingPrefs(false);
}

// Verify that prefs are writen when writing of the prefs is enabled via field
// trial.
IN_PROC_BROWSER_TEST_F(UINetworkQualityEstimatorServiceBrowserTest,
                       WritingToPrefsEnabled) {
  VerifyWritingReadingPrefs(true);
}
