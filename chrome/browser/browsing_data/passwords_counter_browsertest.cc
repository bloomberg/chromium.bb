// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/passwords_counter.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/common/password_form.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

using autofill::PasswordForm;

// TODO(crbug.com/553421): Only RestartOnSyncChange is a SyncTest.
// Extract it together with HistoryCounterTest.RestartOnSyncChange.
class PasswordsCounterTest : public SyncTest {
 public:
  PasswordsCounterTest() : SyncTest(SINGLE_CLIENT) {}

  void SetUpOnMainThread() override {
    finished_ = false;
    time_ = base::Time::Now();
    store_ = PasswordStoreFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
    SetPasswordsDeletionPref(true);
    SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  }

  void AddLogin(const std::string& origin,
                const std::string& username,
                bool blacklisted) {
    // Add login and wait until the password store actually changes.
    // on the database thread.
    passwords_helper::AddLogin(
        store_.get(), CreateCredentials(origin, username, blacklisted));
  }

  void RemoveLogin(const std::string& origin,
                   const std::string& username,
                   bool blacklisted) {
    // Remove login and wait until the password store actually changes
    // on the database thread.
    passwords_helper::RemoveLogin(
        store_.get(), CreateCredentials(origin, username, blacklisted));
  }

  void SetPasswordsDeletionPref(bool value) {
    browser()->profile()->GetPrefs()->SetBoolean(
        browsing_data::prefs::kDeletePasswords, value);
  }

  void SetDeletionPeriodPref(browsing_data::TimePeriod period) {
    browser()->profile()->GetPrefs()->SetInteger(
        browsing_data::prefs::kDeleteTimePeriod, static_cast<int>(period));
  }

  void RevertTimeInDays(int days) {
    time_ -= base::TimeDelta::FromDays(days);
  }

  void WaitForCounting() {
    // The counting takes place on the database thread. Wait until it finishes.
    base::WaitableEvent waitable_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    store_->ScheduleTask(base::Bind(&base::WaitableEvent::Signal,
                                    base::Unretained(&waitable_event)));
    waitable_event.Wait();

    // At this point, the calculation on DB thread should have finished, and
    // a callback should be scheduled on the UI thread. Process the tasks until
    // we get a finished result.
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

  browsing_data::BrowsingDataCounter::ResultInt GetResult() {
    DCHECK(finished_);
    return result_;
  }

  bool PasswordSyncEnabled() { return password_sync_enabled_; }

  void Callback(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
    DCHECK(result);
    finished_ = result->Finished();

    if (finished_) {
      auto* password_result =
          static_cast<browsing_data::PasswordsCounter::PasswordResult*>(
              result.get());
      result_ = password_result->Value();

      password_sync_enabled_ = password_result->password_sync_enabled();
    }
    if (run_loop_ && finished_)
      run_loop_->Quit();
  }

  void WaitForUICallbacksFromAddingLogins() {
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

 private:
  PasswordForm CreateCredentials(const std::string& origin,
                                 const std::string& username,
                                 bool blacklisted) {
    PasswordForm result;
    result.signon_realm = origin;
    result.origin = GURL(origin);
    result.username_value = base::ASCIIToUTF16(username);
    result.password_value = base::ASCIIToUTF16("hunter2");
    result.blacklisted_by_user = blacklisted;
    result.date_created = time_;
    return result;
  }

  scoped_refptr<password_manager::PasswordStore> store_;

  std::unique_ptr<base::RunLoop> run_loop_;
  base::Time time_;

  bool finished_;
  browsing_data::BrowsingDataCounter::ResultInt result_;
  bool password_sync_enabled_;
};

// Tests that the counter correctly counts each individual credential on
// the same domain.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, SameDomain) {
  AddLogin("https://www.google.com", "user1", false);
  AddLogin("https://www.google.com", "user2", false);
  AddLogin("https://www.google.com", "user3", false);
  AddLogin("https://www.chrome.com", "user1", false);
  AddLogin("https://www.chrome.com", "user2", false);
  WaitForUICallbacksFromAddingLogins();

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS),
      ProfileSyncServiceFactory::GetForProfile(profile));
  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&PasswordsCounterTest::Callback, base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  EXPECT_EQ(5u, GetResult());
  EXPECT_FALSE(PasswordSyncEnabled());
}

// Tests that the counter doesn't count blacklisted entries.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, Blacklisted) {
  AddLogin("https://www.google.com", "user1", false);
  AddLogin("https://www.google.com", "user2", true);
  AddLogin("https://www.chrome.com", "user3", true);
  WaitForUICallbacksFromAddingLogins();

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS),
      ProfileSyncServiceFactory::GetForProfile(profile));

  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&PasswordsCounterTest::Callback, base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  EXPECT_EQ(1u, GetResult());
  EXPECT_FALSE(PasswordSyncEnabled());
}

// Tests that the counter starts counting automatically when the deletion
// pref changes to true.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, PrefChanged) {
  SetPasswordsDeletionPref(false);
  AddLogin("https://www.google.com", "user", false);
  AddLogin("https://www.chrome.com", "user", false);
  WaitForUICallbacksFromAddingLogins();

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS),
      ProfileSyncServiceFactory::GetForProfile(profile));
  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&PasswordsCounterTest::Callback, base::Unretained(this)));
  SetPasswordsDeletionPref(true);

  WaitForCounting();
  EXPECT_EQ(2u, GetResult());
  EXPECT_FALSE(PasswordSyncEnabled());
}

// Tests that the counter starts counting automatically when
// the password store changes.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, StoreChanged) {
  AddLogin("https://www.google.com", "user", false);
  WaitForUICallbacksFromAddingLogins();

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS),
      ProfileSyncServiceFactory::GetForProfile(profile));
  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&PasswordsCounterTest::Callback, base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  EXPECT_EQ(1u, GetResult());

  AddLogin("https://www.chrome.com", "user", false);
  WaitForCounting();
  EXPECT_EQ(2u, GetResult());

  RemoveLogin("https://www.chrome.com", "user", false);
  WaitForCounting();
  EXPECT_EQ(1u, GetResult());
}

// Tests that changing the deletion period restarts the counting, and that
// the result takes login creation dates into account.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, PeriodChanged) {
  AddLogin("https://www.google.com", "user", false);
  RevertTimeInDays(2);
  AddLogin("https://example.com", "user1", false);
  RevertTimeInDays(3);
  AddLogin("https://example.com", "user2", false);
  RevertTimeInDays(30);
  AddLogin("https://www.chrome.com", "user", false);
  WaitForUICallbacksFromAddingLogins();

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS),
      ProfileSyncServiceFactory::GetForProfile(profile));
  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&PasswordsCounterTest::Callback, base::Unretained(this)));

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_HOUR);
  WaitForCounting();
  EXPECT_EQ(1u, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_DAY);
  WaitForCounting();
  EXPECT_EQ(1u, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_WEEK);
  WaitForCounting();
  EXPECT_EQ(3u, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::FOUR_WEEKS);
  WaitForCounting();
  EXPECT_EQ(3u, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  WaitForCounting();
  EXPECT_EQ(4u, GetResult());
}

// Test that the counting restarts when password sync state changes.
// TODO(crbug.com/553421): Move this to the sync/test/integration directory?
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, RestartOnSyncChange) {
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
      base::Bind(&PasswordsCounterTest::Callback, base::Unretained(this)));

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
  EXPECT_TRUE(PasswordSyncEnabled());

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
  EXPECT_FALSE(PasswordSyncEnabled());

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
  EXPECT_TRUE(PasswordSyncEnabled());

  // Stopping the Sync service triggers a restart.
  sync_service->RequestStop(syncer::SyncService::CLEAR_DATA);
  WaitForCountingOrConfirmFinished();
  EXPECT_FALSE(PasswordSyncEnabled());
}

}  // namespace
