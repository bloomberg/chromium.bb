// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "components/unified_consent/scoped_unified_consent.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "components/unified_consent/unified_consent_service.h"

namespace unified_consent {
namespace {

class UnifiedConsentBrowserTest : public SyncTest {
 public:
  UnifiedConsentBrowserTest(UnifiedConsentFeatureState feature_state =
                                UnifiedConsentFeatureState::kEnabled)
      : SyncTest(SINGLE_CLIENT), scoped_unified_consent_state_(feature_state) {}
  ~UnifiedConsentBrowserTest() override = default;

  void EnableSync() {
    Profile* profile = browser()->profile();
    ProfileSyncServiceFactory::GetForProfile(profile)
        ->OverrideNetworkResourcesForTest(
            std::make_unique<fake_server::FakeServerNetworkResources>(
                GetFakeServer()->AsWeakPtr()));

    std::unique_ptr<ProfileSyncServiceHarness> harness =
        ProfileSyncServiceHarness::Create(
            profile, "user@gmail.com", "fake_password",
            ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN);
    EXPECT_TRUE(harness->SetupSync());
  }

  UnifiedConsentService* consent_service() {
    return UnifiedConsentServiceFactory::GetForProfile(browser()->profile());
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  ScopedUnifiedConsent scoped_unified_consent_state_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedConsentBrowserTest);
};

class UnifiedConsentDisabledBrowserTest : public UnifiedConsentBrowserTest {
 public:
  UnifiedConsentDisabledBrowserTest()
      : UnifiedConsentBrowserTest(UnifiedConsentFeatureState::kDisabled) {}
  ~UnifiedConsentDisabledBrowserTest() override = default;

  DISALLOW_COPY_AND_ASSIGN(UnifiedConsentDisabledBrowserTest);
};

// Tests that the settings histogram is recorded if unified consent is enabled.
// The histogram is recorded during profile initialization.
IN_PROC_BROWSER_TEST_F(UnifiedConsentBrowserTest, SettingsHistogram_None) {
  histogram_tester_.ExpectUniqueSample(
      "UnifiedConsent.SyncAndGoogleServicesSettings",
      metrics::SettingsHistogramValue::kNone, 1);
}

// Tests that the settings histogram is recorded if unified consent is disabled.
// The histogram is recorded during profile initialization.
IN_PROC_BROWSER_TEST_F(UnifiedConsentDisabledBrowserTest,
                       SettingsHistogram_None) {
  histogram_tester_.ExpectUniqueSample(
      "UnifiedConsent.SyncAndGoogleServicesSettings",
      metrics::SettingsHistogramValue::kNone, 1);
}

// Tests that all service entries in the settings histogram are recorded after
// enabling them.
IN_PROC_BROWSER_TEST_F(
    UnifiedConsentBrowserTest,
    PRE_SettingsHistogram_UrlKeyedAnonymizedDataCollectionEnabled) {
  EnableSync();
  consent_service()->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
}

IN_PROC_BROWSER_TEST_F(
    UnifiedConsentBrowserTest,
    SettingsHistogram_UrlKeyedAnonymizedDataCollectionEnabled) {
  histogram_tester_.ExpectBucketCount(
      "UnifiedConsent.SyncAndGoogleServicesSettings",
      metrics::SettingsHistogramValue::kNone, 0);
  histogram_tester_.ExpectBucketCount(
      "UnifiedConsent.SyncAndGoogleServicesSettings",
      metrics::SettingsHistogramValue::kUrlKeyedAnonymizedDataCollection, 1);
  histogram_tester_.ExpectTotalCount(
      "UnifiedConsent.SyncAndGoogleServicesSettings", 1);
}

}  // namespace
}  // namespace unified_consent
