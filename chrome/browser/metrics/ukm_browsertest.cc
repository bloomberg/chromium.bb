// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "components/ukm/ukm_service.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/test_utils.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager_base.h"
#endif

namespace metrics {

namespace {

// Clears the specified data using BrowsingDataRemover.
void ClearBrowsingData(Profile* profile) {
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(profile);
  content::BrowsingDataRemoverCompletionObserver observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
  observer.BlockUntilCompletion();
  // Make sure HistoryServiceObservers have a chance to be notified.
  content::RunAllTasksUntilIdle();
}

}  // namespace

// An observer that returns back to test code after a new profile is
// initialized.
void UnblockOnProfileCreation(base::RunLoop* run_loop,
                              Profile* profile,
                              Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    run_loop->Quit();
}

Profile* CreateNonSyncProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      new_path, base::Bind(&UnblockOnProfileCreation, &run_loop),
      base::string16(), std::string(), std::string());
  run_loop.Run();
  return profile_manager->GetProfileByPath(new_path);
}

// Test fixture that provides access to some UKM internals.
class UkmBrowserTest : public SyncTest {
 public:
  UkmBrowserTest() : SyncTest(SINGLE_CLIENT) {}

  void SetUp() override {
    // Explicitly enable UKM and disable the MetricsReporting (which should
    // not affect UKM).
    scoped_feature_list_.InitWithFeatures({ukm::kUkmFeature},
                                          {internal::kMetricsReportingFeature});
    SyncTest::SetUp();
  }

  bool ukm_enabled() const {
    auto* service = ukm_service();
    return service ? service->recording_enabled_ : false;
  }
  uint64_t client_id() const {
    auto* service = ukm_service();
    return service ? service->client_id_ : 0;
  }
  bool has_source_data() const {
    auto* service = ukm_service();
    return service ? !service->sources().empty() : false;
  }
  void RecordDummySource() {
    auto* service = ukm_service();
    if (service)
      service->UpdateSourceURL(1, GURL("http://example.com"));
  }

 protected:
  std::unique_ptr<ProfileSyncServiceHarness> EnableSyncForProfile(
      Profile* profile) {
    browser_sync::ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);

    sync_service->OverrideNetworkResourcesForTest(
        base::MakeUnique<fake_server::FakeServerNetworkResources>(
            GetFakeServer()->AsWeakPtr()));

#if defined(OS_CHROMEOS)
    // In browser tests, the profile is already authenticated with stub account
    // |user_manager::kStubUserEmail|.
    AccountInfo info = SigninManagerFactory::GetForProfile(profile)
                           ->GetAuthenticatedAccountInfo();
    std::string username = info.email;
    std::string gaia_id = info.gaia;
#else
    std::string username = "user@gmail.com";
    std::string gaia_id = "123456789";
#endif
    std::unique_ptr<ProfileSyncServiceHarness> harness =
        ProfileSyncServiceHarness::Create(
            profile, username, gaia_id, "unused" /* password */,
            ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN);
    EXPECT_TRUE(harness->SetupSync());
    return harness;
  }

 private:
  ukm::UkmService* ukm_service() const {
    return g_browser_process->GetMetricsServicesManager()->GetUkmService();
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

class UkmEnabledChecker : public SingleClientStatusChangeChecker {
 public:
  UkmEnabledChecker(UkmBrowserTest* test,
                    browser_sync::ProfileSyncService* service,
                    bool want_enabled)
      : SingleClientStatusChangeChecker(service),
        test_(test),
        want_enabled_(want_enabled) {}

  // StatusChangeChecker:
  bool IsExitConditionSatisfied() override {
    return test_->ukm_enabled() == want_enabled_;
  }
  std::string GetDebugMessage() const override {
    return std::string("waiting for ukm_enabled=") +
           (want_enabled_ ? "true" : "false");
  }

 private:
  UkmBrowserTest* const test_;
  const bool want_enabled_;
};

// Make sure that UKM is disabled while an incognito window is open.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, RegularPlusIncognitoCheck) {
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

  // Opening another regular browser mustn't enable UKM.
  Browser* regular_browser = CreateBrowser(profile);
  EXPECT_FALSE(ukm_enabled());

  // Opening and closing another Incognito browser mustn't enable UKM.
  CloseBrowserSynchronously(CreateIncognitoBrowser());
  EXPECT_FALSE(ukm_enabled());

  CloseBrowserSynchronously(regular_browser);
  EXPECT_FALSE(ukm_enabled());

  CloseBrowserSynchronously(incognito_browser);
  EXPECT_TRUE(ukm_enabled());
  // Client ID should not have been reset.
  EXPECT_EQ(original_client_id, client_id());

  harness->service()->RequestStop(browser_sync::ProfileSyncService::CLEAR_DATA);
  CloseBrowserSynchronously(sync_browser);
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

// Make sure opening a real window after Incognito doesn't enable UKM.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, IncognitoPlusRegularCheck) {
  // Enable metrics recording and update MetricsServicesManager.
  bool metrics_enabled = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &metrics_enabled);
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(
      false);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_FALSE(ukm_enabled());

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_FALSE(ukm_enabled());

  CloseBrowserSynchronously(incognito_browser);
  EXPECT_TRUE(ukm_enabled());

  harness->service()->RequestStop(browser_sync::ProfileSyncService::CLEAR_DATA);
  CloseBrowserSynchronously(sync_browser);
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

// Make sure that UKM is disabled while an non-sync profile's window is open.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, NonSyncCheck) {
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

  Profile* nonsync_profile = CreateNonSyncProfile();
  Browser* nonsync_browser = CreateBrowser(nonsync_profile);
  EXPECT_FALSE(ukm_enabled());

  CloseBrowserSynchronously(nonsync_browser);
  // TODO(crbug/746076): UKM doesn't actually get re-enabled yet.
  // EXPECT_TRUE(ukm_enabled());
  // Client ID should not have been reset.
  EXPECT_EQ(original_client_id, client_id());

  harness->service()->RequestStop(browser_sync::ProfileSyncService::CLEAR_DATA);
  CloseBrowserSynchronously(sync_browser);
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

// Make sure that UKM is disabled when metrics consent is revoked.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, MetricsConsentCheck) {
  // Enable metrics recording and update MetricsServicesManager.
  bool metrics_enabled = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &metrics_enabled);
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();

  metrics_enabled = false;
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);

  EXPECT_FALSE(ukm_enabled());

  metrics_enabled = true;
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);

  EXPECT_TRUE(ukm_enabled());
  // Client ID should have been reset.
  EXPECT_NE(original_client_id, client_id());

  harness->service()->RequestStop(browser_sync::ProfileSyncService::CLEAR_DATA);
  CloseBrowserSynchronously(sync_browser);
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

// Make sure that providing consent doesn't enable UKM when sync is disabled.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, ConsentAddedButNoSyncCheck) {
  // Enable metrics recording and update MetricsServicesManager.
  bool metrics_enabled = false;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &metrics_enabled);
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  Browser* browser = CreateBrowser(profile);
  EXPECT_FALSE(ukm_enabled());

  metrics_enabled = true;
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);
  EXPECT_FALSE(ukm_enabled());

  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);
  EXPECT_TRUE(ukm_enabled());

  harness->service()->RequestStop(browser_sync::ProfileSyncService::CLEAR_DATA);
  CloseBrowserSynchronously(browser);
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

// Make sure that UKM is disabled when an open sync window disables it.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, DisableSyncCheck) {
  // Enable metrics recording and update MetricsServicesManager.
  bool metrics_enabled = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &metrics_enabled);
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();

  harness->DisableSyncForDatatype(syncer::TYPED_URLS);
  EXPECT_FALSE(ukm_enabled());

  harness->EnableSyncForDatatype(syncer::TYPED_URLS);
  EXPECT_TRUE(ukm_enabled());
  // Client ID should be reset.
  EXPECT_NE(original_client_id, client_id());

  harness->service()->RequestStop(browser_sync::ProfileSyncService::CLEAR_DATA);
  CloseBrowserSynchronously(sync_browser);
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

// Make sure that UKM is disabled when an secondary passphrase is set.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, SecondaryPassphraseCheck) {
  // Enable metrics recording and update MetricsServicesManager.
  bool metrics_enabled = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &metrics_enabled);
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();

  // Setting an encryption passphrase is done on the "sync" thread meaning the
  // method only posts a task and returns. That task, when executed, will
  // set the passphrase and notify observers (which disables UKM).
  harness->service()->SetEncryptionPassphrase("foo",
                                              syncer::SyncService::EXPLICIT);
  UkmEnabledChecker checker(this, harness->service(), false);
  EXPECT_TRUE(checker.Wait());

  // Client ID should be reset.
  EXPECT_NE(original_client_id, client_id());

  harness->service()->RequestStop(browser_sync::ProfileSyncService::CLEAR_DATA);
  CloseBrowserSynchronously(sync_browser);
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

// Make sure that UKM is disabled when user signs out of Sync.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, SyncSignoutCheck) {
  // Enable metrics recording and update MetricsServicesManager.
  bool metrics_enabled = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &metrics_enabled);
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());

  harness->SignoutSyncService();
  EXPECT_FALSE(ukm_enabled());

  harness->service()->RequestStop(browser_sync::ProfileSyncService::CLEAR_DATA);
  CloseBrowserSynchronously(sync_browser);
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

// Make sure that UKM is not affected by MetricsReporting Feature (sampling).
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, MetricsReportingCheck) {
  // Need to set the Metrics Default to OPT_OUT to trigger MetricsReporting.
  DCHECK(g_browser_process);
  PrefService* local_state = g_browser_process->local_state();
  metrics::ForceRecordMetricsReportingDefaultState(
      local_state, metrics::EnableMetricsDefault::OPT_OUT);
  // Verify that kMetricsReportingFeature is disabled (i.e. other metrics
  // services will be sampled out).
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(internal::kMetricsReportingFeature));

  // Enable metrics recording and update MetricsServicesManager.
  bool metrics_enabled = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &metrics_enabled);
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());

  harness->service()->RequestStop(browser_sync::ProfileSyncService::CLEAR_DATA);
  CloseBrowserSynchronously(sync_browser);
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

// TODO(crbug/745939): Add a tests for disable w/ multiprofiles.

// TODO(crbug/745939): Add a tests for guest profile.

// Make sure that pending data is deleted when user deletes history.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, HistoryDeleteCheck) {
  // Enable metrics recording and update MetricsServicesManager.
  bool metrics_enabled = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &metrics_enabled);
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();

  RecordDummySource();
  EXPECT_TRUE(has_source_data());

  ClearBrowsingData(profile);
  EXPECT_FALSE(has_source_data());
  // Client ID should NOT be reset.
  EXPECT_EQ(original_client_id, client_id());
  EXPECT_TRUE(ukm_enabled());

  harness->service()->RequestStop(browser_sync::ProfileSyncService::CLEAR_DATA);
  CloseBrowserSynchronously(sync_browser);
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

}  // namespace metrics
