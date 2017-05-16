// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"

using browsing_data::BrowsingDataCounter;

namespace {

// A test for the sync behavior of several BrowsingDataCounters.
class SyncAwareCounterTest : public SyncTest {
 public:
  SyncAwareCounterTest() : SyncTest(SINGLE_CLIENT) {}
  ~SyncAwareCounterTest() override {}

  void SetUpOnMainThread() override {
    fake_web_history_service_.reset(new history::FakeWebHistoryService(
        ProfileOAuth2TokenServiceFactory::GetForProfile(browser()->profile()),
        SigninManagerFactory::GetForProfile(browser()->profile()),
        browser()->profile()->GetRequestContext()));
  }

  history::WebHistoryService* GetFakeWebHistoryService(Profile* profile) {
    // Check if history sync is enabled.
    if (WebHistoryServiceFactory::GetForProfile(profile)) {
      return fake_web_history_service_.get();
    }
    return nullptr;
  }

  // Callback and result retrieval ---------------------------------------------

  void WaitForCounting() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  bool CountingFinishedSinceLastAsked() {
    bool result = finished_;
    finished_ = false;
    return result;
  }

  void WaitForCountingOrConfirmFinished() {
    if (CountingFinishedSinceLastAsked())
      return;

    WaitForCounting();
    CountingFinishedSinceLastAsked();
  }

  bool IsSyncEnabled() { return sync_enabled_; }

  void Callback(std::unique_ptr<BrowsingDataCounter::Result> result) {
    finished_ = result->Finished();

    if (finished_) {
      auto* sync_result =
          static_cast<BrowsingDataCounter::SyncResult*>(result.get());
      sync_enabled_ = sync_result->is_sync_enabled();
    }

    if (run_loop_ && finished_)
      run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<history::FakeWebHistoryService> fake_web_history_service_;

  bool finished_;
  bool sync_enabled_;

  DISALLOW_COPY_AND_ASSIGN(SyncAwareCounterTest);
};

// Test that the counting restarts when autofill sync state changes.
// TODO(crbug.com/553421): Move this to the sync/test/integration directory?
IN_PROC_BROWSER_TEST_F(SyncAwareCounterTest, AutofillCounter) {
  // Set up the Sync client.
  ASSERT_TRUE(SetupClients());
  static const int kFirstProfileIndex = 0;
  browser_sync::ProfileSyncService* sync_service =
      GetSyncService(kFirstProfileIndex);
  Profile* profile = GetProfile(kFirstProfileIndex);
  // Set up the counter.
  browsing_data::AutofillCounter counter(
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile, ServiceAccessType::IMPLICIT_ACCESS),
      sync_service);

  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&SyncAwareCounterTest::Callback, base::Unretained(this)));

  // Note that some Sync operations notify observers immediately (and thus there
  // is no need to call |WaitForCounting()|; in fact, it would block the test),
  // while other operations only post the task on UI thread's message loop
  // (which requires calling |WaitForCounting()| for them to run). Therefore,
  // this test always checks if the callback has already run and only waits
  // if it has not.

  // We sync all datatypes by default, so starting Sync means that we start
  // syncing autofill, and this should restart the counter.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_service->IsSyncActive());
  ASSERT_TRUE(sync_service->GetActiveDataTypes().Has(syncer::AUTOFILL));
  WaitForCountingOrConfirmFinished();
  EXPECT_TRUE(IsSyncEnabled());

  // We stop syncing autofill in particular. This restarts the counter.
  syncer::ModelTypeSet everything_except_autofill =
      syncer::UserSelectableTypes();
  everything_except_autofill.Remove(syncer::AUTOFILL);
  auto sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->OnUserChoseDatatypes(/*sync_everything=*/false,
                                     everything_except_autofill);
  ASSERT_FALSE(sync_service->GetPreferredDataTypes().Has(syncer::AUTOFILL));
  sync_blocker.reset();
  WaitForCountingOrConfirmFinished();
  ASSERT_FALSE(sync_service->GetActiveDataTypes().Has(syncer::AUTOFILL));
  EXPECT_FALSE(IsSyncEnabled());

  // If autofill sync is not affected, the counter is not restarted.
  syncer::ModelTypeSet only_history(syncer::HISTORY_DELETE_DIRECTIVES);
  sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->ChangePreferredDataTypes(only_history);
  sync_blocker.reset();
  EXPECT_FALSE(CountingFinishedSinceLastAsked());

  // We start syncing autofill again. This restarts the counter.
  sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->ChangePreferredDataTypes(syncer::ModelTypeSet::All());
  sync_blocker.reset();
  WaitForCountingOrConfirmFinished();
  EXPECT_TRUE(IsSyncEnabled());

  // Stopping the Sync service triggers a restart.
  sync_service->RequestStop(syncer::SyncService::CLEAR_DATA);
  WaitForCountingOrConfirmFinished();
  EXPECT_FALSE(IsSyncEnabled());
}

// Test that the counting restarts when password sync state changes.
// TODO(crbug.com/553421): Move this to the sync/test/integration directory?
IN_PROC_BROWSER_TEST_F(SyncAwareCounterTest, PasswordCounter) {
  // Set up the Sync client.
  ASSERT_TRUE(SetupClients());
  static const int kFirstProfileIndex = 0;
  browser_sync::ProfileSyncService* sync_service =
      GetSyncService(kFirstProfileIndex);
  Profile* profile = GetProfile(kFirstProfileIndex);
  // Set up the counter.
  browsing_data::PasswordsCounter counter(
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS),
      sync_service);

  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&SyncAwareCounterTest::Callback, base::Unretained(this)));

  // Note that some Sync operations notify observers immediately (and thus there
  // is no need to call |WaitForCounting()|; in fact, it would block the test),
  // while other operations only post the task on UI thread's message loop
  // (which requires calling |WaitForCounting()| for them to run). Therefore,
  // this test always checks if the callback has already run and only waits
  // if it has not.

  // We sync all datatypes by default, so starting Sync means that we start
  // syncing passwords, and this should restart the counter.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_service->IsSyncActive());
  ASSERT_TRUE(sync_service->GetPreferredDataTypes().Has(syncer::PASSWORDS));
  WaitForCountingOrConfirmFinished();
  EXPECT_TRUE(IsSyncEnabled());

  // We stop syncing passwords in particular. This restarts the counter.
  syncer::ModelTypeSet everything_except_passwords =
      syncer::UserSelectableTypes();
  everything_except_passwords.Remove(syncer::PASSWORDS);
  auto sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->OnUserChoseDatatypes(/*sync_everything=*/false,
                                     everything_except_passwords);
  ASSERT_FALSE(sync_service->GetPreferredDataTypes().Has(syncer::PASSWORDS));
  sync_blocker.reset();
  WaitForCountingOrConfirmFinished();
  ASSERT_FALSE(sync_service->GetPreferredDataTypes().Has(syncer::PASSWORDS));
  EXPECT_FALSE(IsSyncEnabled());

  // If password sync is not affected, the counter is not restarted.
  syncer::ModelTypeSet only_history(syncer::HISTORY_DELETE_DIRECTIVES);
  sync_service->ChangePreferredDataTypes(only_history);
  sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->ChangePreferredDataTypes(only_history);
  sync_blocker.reset();
  EXPECT_FALSE(CountingFinishedSinceLastAsked());

  // We start syncing passwords again. This restarts the counter.
  sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->ChangePreferredDataTypes(syncer::ModelTypeSet::All());
  sync_blocker.reset();
  WaitForCountingOrConfirmFinished();
  EXPECT_TRUE(IsSyncEnabled());

  // Stopping the Sync service triggers a restart.
  sync_service->RequestStop(syncer::SyncService::CLEAR_DATA);
  WaitForCountingOrConfirmFinished();
  EXPECT_FALSE(IsSyncEnabled());
}

// Test that the counting restarts when history sync state changes.
// TODO(crbug.com/553421): Move this to the sync/test/integration directory?
IN_PROC_BROWSER_TEST_F(SyncAwareCounterTest, HistoryCounter) {
  // Set up the Sync client.
  ASSERT_TRUE(SetupClients());
  static const int kFirstProfileIndex = 0;
  browser_sync::ProfileSyncService* sync_service =
      GetSyncService(kFirstProfileIndex);
  Profile* profile = GetProfile(kFirstProfileIndex);

  // Set up the fake web history service and the counter.

  browsing_data::HistoryCounter counter(
      HistoryServiceFactory::GetForProfileWithoutCreating(browser()->profile()),
      base::Bind(&SyncAwareCounterTest::GetFakeWebHistoryService,
                 base::Unretained(this), base::Unretained(profile)),
      sync_service);

  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&SyncAwareCounterTest::Callback, base::Unretained(this)));

  // Note that some Sync operations notify observers immediately (and thus there
  // is no need to call |WaitForCounting()|; in fact, it would block the test),
  // while other operations only post the task on UI thread's message loop
  // (which requires calling |WaitForCounting()| for them to run). Therefore,
  // this test always checks if the callback has already run and only waits
  // if it has not.

  // We sync all datatypes by default, so starting Sync means that we start
  // syncing history deletion, and this should restart the counter.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_service->IsSyncActive());
  ASSERT_TRUE(sync_service->GetPreferredDataTypes().Has(
      syncer::HISTORY_DELETE_DIRECTIVES));
  ASSERT_TRUE(sync_service->GetActiveDataTypes().Has(
      syncer::HISTORY_DELETE_DIRECTIVES));

  WaitForCountingOrConfirmFinished();
  EXPECT_TRUE(IsSyncEnabled());

  // We stop syncing history deletion in particular. This restarts the counter.
  syncer::ModelTypeSet everything_except_history =
      syncer::UserSelectableTypes();
  everything_except_history.Remove(syncer::HISTORY_DELETE_DIRECTIVES);
  auto sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->OnUserChoseDatatypes(/*sync_everything=*/false,
                                     everything_except_history);
  sync_blocker.reset();
  WaitForCountingOrConfirmFinished();
  EXPECT_FALSE(IsSyncEnabled());

  // If the history deletion sync is not affected, the counter is not restarted.
  syncer::ModelTypeSet only_passwords(syncer::PASSWORDS);
  sync_service->ChangePreferredDataTypes(only_passwords);
  sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->ChangePreferredDataTypes(only_passwords);
  sync_blocker.reset();
  EXPECT_FALSE(counter.HasTrackedTasks());
  EXPECT_FALSE(CountingFinishedSinceLastAsked());

  // Same in this case.
  syncer::ModelTypeSet autofill_and_passwords(syncer::AUTOFILL,
                                              syncer::PASSWORDS);
  sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->ChangePreferredDataTypes(autofill_and_passwords);
  sync_blocker.reset();
  EXPECT_FALSE(counter.HasTrackedTasks());
  EXPECT_FALSE(CountingFinishedSinceLastAsked());

  // We start syncing history deletion again. This restarts the counter.
  sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->ChangePreferredDataTypes(syncer::ModelTypeSet::All());
  sync_blocker.reset();
  WaitForCountingOrConfirmFinished();
  EXPECT_TRUE(IsSyncEnabled());

  // Changing the syncing datatypes to another set that still includes history
  // deletion should technically not trigger a restart, because the state of
  // history deletion did not change. However, in reality we can get two
  // notifications, one that history sync has stopped and another that it is
  // active again.

  // Stopping the Sync service triggers a restart.
  sync_service->RequestStop(syncer::SyncService::CLEAR_DATA);
  WaitForCountingOrConfirmFinished();
  EXPECT_FALSE(IsSyncEnabled());
}

}  // namespace
