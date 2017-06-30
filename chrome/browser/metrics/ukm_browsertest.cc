// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "components/ukm/public/ukm_recorder.h"
#include "components/ukm/ukm_service.h"
#include "content/public/common/content_switches.h"

namespace metrics {

// Test fixture that provides access to some UKM internals.
class UkmBrowserTest : public SyncTest {
 public:
  UkmBrowserTest() : SyncTest(SINGLE_CLIENT) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SyncTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                    ukm::kUkmFeature.name);
  }

 protected:
  bool ukm_enabled() {
    auto* service = ukm_service();
    return service ? service->recording_enabled_ : false;
  }
  uint64_t client_id() {
    auto* service = ukm_service();
    return service ? service->client_id_ : 0;
  }

  std::unique_ptr<ProfileSyncServiceHarness> EnableSyncForProfile(
      Profile* profile) {
    browser_sync::ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);

    sync_service->OverrideNetworkResourcesForTest(
        base::MakeUnique<fake_server::FakeServerNetworkResources>(
            GetFakeServer()->AsWeakPtr()));

    std::unique_ptr<ProfileSyncServiceHarness> harness =
        ProfileSyncServiceHarness::Create(
            profile, "user@gmail.com", "password",
            ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN);

    EXPECT_TRUE(harness->SetupSync());
    return harness;
  }

 private:
  ukm::UkmService* ukm_service() {
    return static_cast<ukm::UkmService*>(ukm::UkmRecorder::Get());
  }
};

// Make sure that UKM is disabled while an incognito window is open.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, IncognitoCheck) {
  // Enable metrics recording and update MetricsServicesManager.
  bool metrics_enabled = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &metrics_enabled);
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(
      false);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();

  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_FALSE(ukm_enabled());

  CloseBrowserSynchronously(incognito_browser);
  EXPECT_TRUE(ukm_enabled());
  // Client ID should not have been reset.
  EXPECT_EQ(original_client_id, client_id());

  harness->service()->RequestStop(browser_sync::ProfileSyncService::CLEAR_DATA);
  CloseBrowserSynchronously(sync_browser);
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

// TODO(holte): Add more tests to cover multi-profile cases.

}  // namespace metrics
