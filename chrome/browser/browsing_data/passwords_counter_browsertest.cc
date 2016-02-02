// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/passwords_counter.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/common/password_form.h"
#include "components/prefs/pref_service.h"

namespace {

using autofill::PasswordForm;

class PasswordsCounterTest : public InProcessBrowserTest,
                             public password_manager::PasswordStore::Observer {
 public:
  void SetUpOnMainThread() override {
    time_ = base::Time::Now();
    store_ = PasswordStoreFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
    SetPasswordsDeletionPref(true);
    SetDeletionPeriodPref(BrowsingDataRemover::EVERYTHING);
  }

  void AddLogin(const std::string& origin,
                const std::string& username,
                bool blacklisted) {
    // Add login and wait until the password store actually changes.
    // on the database thread.
    passwords_helper::AddLogin(
        store_.get(), CreateCredentials(origin, username, blacklisted));
    base::RunLoop().RunUntilIdle();
    // Even after the store changes on the database thread, we must wait until
    // the listeners are notified on this thread.
    run_loop_.reset(new base::RunLoop());
    run_loop_->RunUntilIdle();
  }

  void RemoveLogin(const std::string& origin,
                   const std::string& username,
                   bool blacklisted) {
    // Remove login and wait until the password store actually changes
    // on the database thread.
    passwords_helper::RemoveLogin(
        store_.get(), CreateCredentials(origin, username, blacklisted));
    // Even after the store changes on the database thread, we must wait until
    // the listeners are notified on this thread.
    run_loop_.reset(new base::RunLoop());
    run_loop_->RunUntilIdle();
  }

  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override {
    run_loop_->Quit();
  }

  void SetPasswordsDeletionPref(bool value) {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kDeletePasswords, value);
  }

  void SetDeletionPeriodPref(BrowsingDataRemover::TimePeriod period) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kDeleteTimePeriod, static_cast<int>(period));
  }

  void RevertTimeInDays(int days) {
    time_ -= base::TimeDelta::FromDays(days);
  }

  void WaitForCounting() {
    if (finished_)
      return;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  BrowsingDataCounter::ResultInt GetResult() {
    DCHECK(finished_);
    return result_;
  }

  void Callback(scoped_ptr<BrowsingDataCounter::Result> result) {
    finished_ = result->Finished();

    if (finished_) {
      result_ = static_cast<BrowsingDataCounter::FinishedResult*>(
          result.get())->Value();
    }

    if (run_loop_ && finished_)
      run_loop_->Quit();
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

  scoped_ptr<base::RunLoop> run_loop_;
  base::Time time_;

  bool finished_;
  BrowsingDataCounter::ResultInt result_;
};

// Tests that the counter correctly counts each individual credential on
// the same domain.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, SameDomain) {
  AddLogin("https://www.google.com", "user1", false);
  AddLogin("https://www.google.com", "user2", false);
  AddLogin("https://www.google.com", "user3", false);
  AddLogin("https://www.chrome.com", "user1", false);
  AddLogin("https://www.chrome.com", "user2", false);

  PasswordsCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&PasswordsCounterTest::Callback,
                          base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  EXPECT_EQ(5u, GetResult());
}

// Tests that the counter doesn't count blacklisted entries.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, Blacklisted) {
  AddLogin("https://www.google.com", "user1", false);
  AddLogin("https://www.google.com", "user2", true);
  AddLogin("https://www.chrome.com", "user3", true);

  PasswordsCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&PasswordsCounterTest::Callback,
                          base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  EXPECT_EQ(1u, GetResult());
}

// Tests that the counter starts counting automatically when the deletion
// pref changes to true.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, PrefChanged) {
  SetPasswordsDeletionPref(false);
  AddLogin("https://www.google.com", "user", false);
  AddLogin("https://www.chrome.com", "user", false);

  PasswordsCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&PasswordsCounterTest::Callback,
                          base::Unretained(this)));
  SetPasswordsDeletionPref(true);

  WaitForCounting();
  EXPECT_EQ(2u, GetResult());
}

// Tests that the counter does not count passwords if the deletion
// preference is false.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, PrefIsFalse) {
  SetPasswordsDeletionPref(false);
  AddLogin("https://www.google.com", "user", false);

  PasswordsCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&PasswordsCounterTest::Callback,
                          base::Unretained(this)));
  counter.Restart();

  EXPECT_FALSE(counter.cancelable_task_tracker()->HasTrackedTasks());
}

// Tests that the counter starts counting automatically when
// the password store changes.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, StoreChanged) {
  AddLogin("https://www.google.com", "user", false);

  PasswordsCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&PasswordsCounterTest::Callback,
                          base::Unretained(this)));
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

  PasswordsCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&PasswordsCounterTest::Callback,
                          base::Unretained(this)));

  SetDeletionPeriodPref(BrowsingDataRemover::LAST_HOUR);
  WaitForCounting();
  EXPECT_EQ(1u, GetResult());

  SetDeletionPeriodPref(BrowsingDataRemover::LAST_DAY);
  WaitForCounting();
  EXPECT_EQ(1u, GetResult());

  SetDeletionPeriodPref(BrowsingDataRemover::LAST_WEEK);
  WaitForCounting();
  EXPECT_EQ(3u, GetResult());

  SetDeletionPeriodPref(BrowsingDataRemover::FOUR_WEEKS);
  WaitForCounting();
  EXPECT_EQ(3u, GetResult());

  SetDeletionPeriodPref(BrowsingDataRemover::EVERYTHING);
  WaitForCounting();
  EXPECT_EQ(4u, GetResult());
}

}  // namespace
