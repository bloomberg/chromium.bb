// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/history_counter.h"

#include "base/run_loop.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace {

class HistoryCounterTest : public SyncTest {
 public:
  HistoryCounterTest() : SyncTest(SINGLE_CLIENT) {
    // TODO(msramek): Only one of the test cases, RestartOnSyncChange, is a Sync
    // integration test. Extract it and move it to the rest of integration tests
    // in chrome/browser/sync/test/integration/. Change this class back to
    // InProcessBrowserTest.
  }

  ~HistoryCounterTest() override {};

  void SetUpOnMainThread() override {
    time_ = base::Time::Now();
    service_ = HistoryServiceFactory::GetForProfileWithoutCreating(
        browser()->profile());
    SetHistoryDeletionPref(true);
    SetDeletionPeriodPref(BrowsingDataRemover::EVERYTHING);
  }

  void AddVisit(const std::string url) {
    service_->AddPage(GURL(url), time_, history::SOURCE_BROWSED);
  }

  const base::Time& GetCurrentTime() {
    return time_;
  }

  void RevertTimeInDays(int days) {
    time_ -= base::TimeDelta::FromDays(days);
  }

  void SetHistoryDeletionPref(bool value) {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kDeleteBrowsingHistory, value);
  }

  void SetDeletionPeriodPref(BrowsingDataRemover::TimePeriod period) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kDeleteTimePeriod, static_cast<int>(period));
  }

  void WaitForCounting() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  BrowsingDataCounter::ResultInt GetLocalResult() {
    DCHECK(finished_);
    return local_result_;
  }

  bool HasSyncedVisits() {
    DCHECK(finished_);
    return has_synced_visits_;
  }

  void Callback(std::unique_ptr<BrowsingDataCounter::Result> result) {
    finished_ = result->Finished();

    if (finished_) {
      HistoryCounter::HistoryResult* history_result =
          static_cast<HistoryCounter::HistoryResult*>(result.get());

      local_result_ = history_result->Value();
      has_synced_visits_ = history_result->has_synced_visits();
    }

    if (run_loop_ && finished_)
      run_loop_->Quit();
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

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  history::HistoryService* service_;
  base::Time time_;

  bool finished_;
  BrowsingDataCounter::ResultInt local_result_;
  bool has_synced_visits_;
};

// Tests that the counter considers duplicate visits from the same day
// to be a single item.
IN_PROC_BROWSER_TEST_F(HistoryCounterTest, DuplicateVisits) {
  AddVisit("https://www.google.com");   // 1 item
  AddVisit("https://www.google.com");
  AddVisit("https://www.chrome.com");   // 2 items
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.example.com");  // 3 items

  RevertTimeInDays(1);
  AddVisit("https://www.google.com");   // 4 items
  AddVisit("https://www.example.com");  // 5 items
  AddVisit("https://www.example.com");

  RevertTimeInDays(1);
  AddVisit("https://www.chrome.com");   // 6 items
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.google.com");   // 7 items
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.google.com");
  AddVisit("https://www.google.com");
  AddVisit("https://www.chrome.com");

  HistoryCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&HistoryCounterTest::Callback,
                          base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  EXPECT_EQ(7u, GetLocalResult());
}

// Tests that the counter starts counting automatically when the deletion
// pref changes to true.
IN_PROC_BROWSER_TEST_F(HistoryCounterTest, PrefChanged) {
  SetHistoryDeletionPref(false);
  AddVisit("https://www.google.com");
  AddVisit("https://www.chrome.com");

  HistoryCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&HistoryCounterTest::Callback,
                          base::Unretained(this)));
  SetHistoryDeletionPref(true);

  WaitForCounting();
  EXPECT_EQ(2u, GetLocalResult());
}

// Tests that the counter does not count history if the deletion
// preference is false.
IN_PROC_BROWSER_TEST_F(HistoryCounterTest, PrefIsFalse) {
  SetHistoryDeletionPref(false);
  AddVisit("https://www.google.com");

  HistoryCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&HistoryCounterTest::Callback,
                          base::Unretained(this)));
  counter.Restart();

  EXPECT_FALSE(counter.HasTrackedTasks());
}

// Tests that changing the deletion period restarts the counting, and that
// the result takes visit dates into account.
IN_PROC_BROWSER_TEST_F(HistoryCounterTest, PeriodChanged) {
  AddVisit("https://www.google.com");

  RevertTimeInDays(2);
  AddVisit("https://www.google.com");
  AddVisit("https://www.example.com");

  RevertTimeInDays(4);
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.example.com");

  RevertTimeInDays(20);
  AddVisit("https://www.google.com");
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.example.com");

  RevertTimeInDays(10);
  AddVisit("https://www.example.com");
  AddVisit("https://www.example.com");
  AddVisit("https://www.example.com");

  HistoryCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&HistoryCounterTest::Callback,
                          base::Unretained(this)));

  SetDeletionPeriodPref(BrowsingDataRemover::LAST_HOUR);
  WaitForCounting();
  EXPECT_EQ(1u, GetLocalResult());

  SetDeletionPeriodPref(BrowsingDataRemover::LAST_DAY);
  WaitForCounting();
  EXPECT_EQ(1u, GetLocalResult());

  SetDeletionPeriodPref(BrowsingDataRemover::LAST_WEEK);
  WaitForCounting();
  EXPECT_EQ(5u, GetLocalResult());

  SetDeletionPeriodPref(BrowsingDataRemover::FOUR_WEEKS);
  WaitForCounting();
  EXPECT_EQ(8u, GetLocalResult());

  SetDeletionPeriodPref(BrowsingDataRemover::EVERYTHING);
  WaitForCounting();
  EXPECT_EQ(9u, GetLocalResult());
}

// Test the behavior for a profile that syncs history.
IN_PROC_BROWSER_TEST_F(HistoryCounterTest, Synced) {
  // WebHistoryService makes network requests, so we need to use a fake one
  // for testing.
  std::unique_ptr<history::FakeWebHistoryService> service(
      new history::FakeWebHistoryService(
          ProfileOAuth2TokenServiceFactory::GetForProfile(browser()->profile()),
          SigninManagerFactory::GetForProfile(browser()->profile()),
          browser()->profile()->GetRequestContext()));

  HistoryCounter counter;
  counter.SetWebHistoryServiceForTesting(service.get());
  counter.Init(browser()->profile(),
               base::Bind(&HistoryCounterTest::Callback,
                          base::Unretained(this)));

  // No entries locally and no entries in Sync.
  service->SetupFakeResponse(true /* success */, net::HTTP_OK);
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(0u, GetLocalResult());
  EXPECT_FALSE(HasSyncedVisits());

  // No entries locally. There are some entries in Sync, but they are out of the
  // time range.
  SetDeletionPeriodPref(BrowsingDataRemover::LAST_HOUR);
  service->AddSyncedVisit(
      "www.google.com", GetCurrentTime() - base::TimeDelta::FromHours(2));
  service->AddSyncedVisit(
      "www.chrome.com", GetCurrentTime() - base::TimeDelta::FromHours(2));
  service->SetupFakeResponse(true /* success */, net::HTTP_OK);
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(0u, GetLocalResult());
  EXPECT_FALSE(HasSyncedVisits());

  // No entries locally, but some entries in Sync.
  service->AddSyncedVisit("www.google.com", GetCurrentTime());
  service->SetupFakeResponse(true /* success */, net::HTTP_OK);
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(0u, GetLocalResult());
  EXPECT_TRUE(HasSyncedVisits());

  // To err on the safe side, if the server request fails, we assume that there
  // might be some items on the server.
  service->SetupFakeResponse(true /* success */,
                             net::HTTP_INTERNAL_SERVER_ERROR);
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(0u, GetLocalResult());
  EXPECT_TRUE(HasSyncedVisits());

  // Same when the entire query fails.
  service->SetupFakeResponse(false /* success */,
                             net::HTTP_INTERNAL_SERVER_ERROR);
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(0u, GetLocalResult());
  EXPECT_TRUE(HasSyncedVisits());

  // Nonzero local count, nonempty sync.
  AddVisit("https://www.google.com");
  AddVisit("https://www.chrome.com");
  service->SetupFakeResponse(true /* success */, net::HTTP_OK);
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(2u, GetLocalResult());
  EXPECT_TRUE(HasSyncedVisits());

  // Nonzero local count, empty sync.
  service->ClearSyncedVisits();
  service->SetupFakeResponse(true /* success */, net::HTTP_OK);
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(2u, GetLocalResult());
  EXPECT_FALSE(HasSyncedVisits());
}

// Test that the counting restarts when history sync state changes.
// TODO(crbug.com/553421): Enable this test and move it to the
// sync/test/integration directory.
IN_PROC_BROWSER_TEST_F(HistoryCounterTest, DISABLED_RestartOnSyncChange) {
  // Set up the Sync client.
  ASSERT_TRUE(SetupClients());
  static const int kFirstProfileIndex = 0;
  ProfileSyncService* sync_service = GetSyncService(kFirstProfileIndex);
  Profile* profile = GetProfile(kFirstProfileIndex);

  // Set up the fake web history service and the counter.
  std::unique_ptr<history::FakeWebHistoryService> web_history_service(
      new history::FakeWebHistoryService(
          ProfileOAuth2TokenServiceFactory::GetForProfile(browser()->profile()),
          SigninManagerFactory::GetForProfile(browser()->profile()),
          browser()->profile()->GetRequestContext()));
  HistoryCounter counter;
  counter.SetWebHistoryServiceForTesting(web_history_service.get());
  counter.Init(profile,
               base::Bind(&HistoryCounterTest::Callback,
                          base::Unretained(this)));

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
  WaitForCountingOrConfirmFinished();

  // We stop syncing history deletion in particular. This restarts the counter.
  syncer::ModelTypeSet everything_except_history = syncer::ModelTypeSet::All();
  everything_except_history.Remove(syncer::HISTORY_DELETE_DIRECTIVES);
  sync_service->SetSetupInProgress(true);
  sync_service->ChangePreferredDataTypes(everything_except_history);
  sync_service->SetSetupInProgress(false);
  WaitForCountingOrConfirmFinished();

  // If the history deletion sync is not affected, the counter is not restarted.
  syncer::ModelTypeSet only_passwords(syncer::PASSWORDS);
  sync_service->ChangePreferredDataTypes(only_passwords);
  sync_service->SetSetupInProgress(true);
  sync_service->ChangePreferredDataTypes(only_passwords);
  sync_service->SetSetupInProgress(false);
  EXPECT_FALSE(counter.HasTrackedTasks());
  EXPECT_FALSE(CountingFinishedSinceLastAsked());

  // Same in this case.
  syncer::ModelTypeSet autofill_and_passwords(
      syncer::AUTOFILL, syncer::PASSWORDS);
  sync_service->SetSetupInProgress(true);
  sync_service->ChangePreferredDataTypes(autofill_and_passwords);
  sync_service->SetSetupInProgress(false);
  EXPECT_FALSE(counter.HasTrackedTasks());
  EXPECT_FALSE(CountingFinishedSinceLastAsked());

  // We start syncing history deletion again. This restarts the counter.
  sync_service->SetSetupInProgress(true);
  sync_service->ChangePreferredDataTypes(syncer::ModelTypeSet::All());
  sync_service->SetSetupInProgress(false);
  WaitForCountingOrConfirmFinished();

  // Changing the syncing datatypes to another set that still includes history
  // deletion should technically not trigger a restart, because the state of
  // history deletion did not change. However, in reality we can get two
  // notifications, one that history sync has stopped and another that it is
  // active again.

  // Stopping the Sync service triggers a restart.
  sync_service->RequestStop(sync_driver::SyncService::CLEAR_DATA);
  WaitForCountingOrConfirmFinished();
}

}  // namespace
