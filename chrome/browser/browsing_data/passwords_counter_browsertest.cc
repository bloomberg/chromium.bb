// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/passwords_counter.h"

#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/common/password_form.h"

namespace {

using autofill::PasswordForm;

class PasswordsCounterTest : public InProcessBrowserTest {
 public:
  void AddLogin(const std::string& origin,
                const std::string& username,
                bool blacklisted) {
    PasswordStoreFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)->
            AddLogin(CreateCredentials(origin, username, blacklisted));
    base::RunLoop().RunUntilIdle();
  }

  void RemoveLogin(const std::string& origin,
                   const std::string& username,
                   bool blacklisted) {
    PasswordStoreFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)->
            RemoveLogin(CreateCredentials(origin, username, blacklisted));
    base::RunLoop().RunUntilIdle();
  }

  void SetPasswordsDeletionPref(bool value) {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kDeletePasswords, value);
  }

  void WaitForCounting() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  uint32 GetResult() {
    DCHECK(finished_);
    return result_;
  }

  void Callback(bool finished, uint32 count) {
    finished_ = finished;
    result_ = count;
    if (run_loop_ && finished)
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
    return result;
  }

  scoped_ptr<base::RunLoop> run_loop_;

  bool finished_;
  uint32 result_;
};

// Tests that the counter correctly counts each individual credential on
// the same domain.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, SameDomain) {
  SetPasswordsDeletionPref(true);
  AddLogin("https://www.google.com", "user1", false);
  AddLogin("https://www.google.com", "user2", false);
  AddLogin("https://www.google.com", "user3", false);
  AddLogin("https://www.chrome.com", "user1", false);
  AddLogin("https://www.chrome.com", "user2", false);

  PasswordsCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&PasswordsCounterTest::Callback,
                          base::Unretained(this)));

  WaitForCounting();
  EXPECT_EQ(5u, GetResult());
}

// Tests that the counter doesn't count blacklisted entries.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, Blacklisted) {
  SetPasswordsDeletionPref(true);
  AddLogin("https://www.google.com", "user1", false);
  AddLogin("https://www.google.com", "user2", true);
  AddLogin("https://www.chrome.com", "user3", true);

  PasswordsCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&PasswordsCounterTest::Callback,
                          base::Unretained(this)));

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

  EXPECT_FALSE(counter.cancelable_task_tracker()->HasTrackedTasks());
}

// Tests that the counter starts counting automatically when
// the password store changes.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, StoreChanged) {
  SetPasswordsDeletionPref(true);
  AddLogin("https://www.google.com", "user", false);

  PasswordsCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&PasswordsCounterTest::Callback,
                          base::Unretained(this)));

  WaitForCounting();
  EXPECT_EQ(1u, GetResult());

  AddLogin("https://www.chrome.com", "user", false);
  WaitForCounting();
  EXPECT_EQ(2u, GetResult());

  RemoveLogin("https://www.chrome.com", "user", false);
  WaitForCounting();
  EXPECT_EQ(1u, GetResult());
}

}  // namespace
