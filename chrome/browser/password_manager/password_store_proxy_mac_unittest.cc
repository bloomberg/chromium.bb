// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_proxy_mac.h"

#include "base/files/scoped_temp_dir.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "components/os_crypt/os_crypt.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "crypto/mock_apple_keychain.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using autofill::PasswordForm;
using content::BrowserThread;
using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pointee;

ACTION(QuitUIMessageLoop) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::MessageLoop::current()->Quit();
}

class MockPasswordStoreConsumer
    : public password_manager::PasswordStoreConsumer {
 public:
  MOCK_METHOD1(OnGetPasswordStoreResultsConstRef,
               void(const std::vector<PasswordForm*>&));

  // GMock cannot mock methods with move-only args.
  void OnGetPasswordStoreResults(ScopedVector<PasswordForm> results) override {
    OnGetPasswordStoreResultsConstRef(results.get());
  }
};

class MockPasswordStoreObserver
    : public password_manager::PasswordStore::Observer {
 public:
  MockPasswordStoreObserver(PasswordStoreProxyMac* password_store)
      : guard_(this) {
    guard_.Add(password_store);
  }
  MOCK_METHOD1(OnLoginsChanged,
               void(const password_manager::PasswordStoreChangeList& changes));

 private:
  ScopedObserver<PasswordStoreProxyMac, MockPasswordStoreObserver> guard_;
};

// A mock LoginDatabase that simulates a failing Init() method.
class BadLoginDatabase : public password_manager::LoginDatabase {
 public:
  BadLoginDatabase() : password_manager::LoginDatabase(base::FilePath()) {}
  ~BadLoginDatabase() override {}

  // LoginDatabase:
  bool Init() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(BadLoginDatabase);
};

class PasswordStoreProxyMacTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

  void CreateAndInitPasswordStore(
      scoped_ptr<password_manager::LoginDatabase> login_db);

  void ClosePasswordStore();

  // Do a store-level query to wait for all the previously enqueued operations
  // to finish.
  void FinishAsyncProcessing();

  // Add/Update/Remove |form| and verify the operation succeeded.
  void AddForm(const PasswordForm& form);
  void UpdateForm(const PasswordForm& form);
  void RemoveForm(const PasswordForm& form);

  // Tests RemoveLoginsCreatedBetween or RemoveLoginsSyncedBetween depending on
  // |check_created|.
  void CheckRemoveLoginsBetween(bool check_created);

  base::FilePath test_login_db_file_path() const;

  password_manager::LoginDatabase* login_db() const {
    return store_->login_metadata_db();
  }

  PasswordStoreProxyMac* store() { return store_.get(); }

 protected:
  content::TestBrowserThreadBundle ui_thread_;

  base::ScopedTempDir db_dir_;
  scoped_refptr<PasswordStoreProxyMac> store_;
};

void PasswordStoreProxyMacTest::SetUp() {
  ASSERT_TRUE(db_dir_.CreateUniqueTempDir());

  // Ensure that LoginDatabase will use the mock keychain if it needs to
  // encrypt/decrypt a password.
  OSCrypt::UseMockKeychain(true);
  scoped_ptr<password_manager::LoginDatabase> login_db(
      new password_manager::LoginDatabase(test_login_db_file_path()));
  CreateAndInitPasswordStore(login_db.Pass());
  // Make sure deferred initialization is performed before some tests start
  // accessing the |login_db| directly.
  FinishAsyncProcessing();
}

void PasswordStoreProxyMacTest::TearDown() {
  ClosePasswordStore();
}

void PasswordStoreProxyMacTest::CreateAndInitPasswordStore(
    scoped_ptr<password_manager::LoginDatabase> login_db) {
  store_ = new PasswordStoreProxyMac(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
      make_scoped_ptr(new crypto::MockAppleKeychain), login_db.Pass());
  ASSERT_TRUE(store_->Init(syncer::SyncableService::StartSyncFlare()));
}

void PasswordStoreProxyMacTest::ClosePasswordStore() {
  if (!store_)
    return;
  store_->Shutdown();
  EXPECT_FALSE(store_->GetBackgroundTaskRunner());
  base::MessageLoop::current()->RunUntilIdle();
  store_ = nullptr;
}

void PasswordStoreProxyMacTest::FinishAsyncProcessing() {
  // Do a store-level query to wait for all the previously enqueued operations
  // to finish.
  MockPasswordStoreConsumer consumer;
  store_->GetLogins(PasswordForm(),
                    password_manager::PasswordStore::ALLOW_PROMPT, &consumer);
  EXPECT_CALL(consumer, OnGetPasswordStoreResultsConstRef(_))
      .WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();
}

base::FilePath PasswordStoreProxyMacTest::test_login_db_file_path() const {
  return db_dir_.path().Append(FILE_PATH_LITERAL("login.db"));
}

void PasswordStoreProxyMacTest::AddForm(const PasswordForm& form) {
  MockPasswordStoreObserver mock_observer(store());

  password_manager::PasswordStoreChangeList list;
  list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD, form));
  EXPECT_CALL(mock_observer, OnLoginsChanged(list));
  store()->AddLogin(form);
  FinishAsyncProcessing();
}

void PasswordStoreProxyMacTest::UpdateForm(const PasswordForm& form) {
  MockPasswordStoreObserver mock_observer(store());

  password_manager::PasswordStoreChangeList list;
  list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::UPDATE, form));
  EXPECT_CALL(mock_observer, OnLoginsChanged(list));
  store()->UpdateLogin(form);
  FinishAsyncProcessing();
}

void PasswordStoreProxyMacTest::RemoveForm(const PasswordForm& form) {
  MockPasswordStoreObserver mock_observer(store());

  password_manager::PasswordStoreChangeList list;
  list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE, form));
  EXPECT_CALL(mock_observer, OnLoginsChanged(list));
  store()->RemoveLogin(form);
  FinishAsyncProcessing();
}

void PasswordStoreProxyMacTest::CheckRemoveLoginsBetween(bool check_created) {
  PasswordForm old_form;
  old_form.origin = GURL("http://accounts.google.com/LoginAuth");
  old_form.signon_realm = "http://accounts.google.com/";
  old_form.username_value = base::ASCIIToUTF16("my_username");
  old_form.federation_url = GURL("http://accounts.google.com/federation");

  PasswordForm new_form = old_form;
  new_form.origin = GURL("http://accounts.google2.com/LoginAuth");
  new_form.signon_realm = "http://accounts.google2.com/";

  base::Time now = base::Time::Now();
  base::Time next_day = now + base::TimeDelta::FromDays(1);
  if (check_created) {
    old_form.date_created = now;
    new_form.date_created = next_day;
  } else {
    old_form.date_synced = now;
    new_form.date_synced = next_day;
  }

  AddForm(old_form);
  AddForm(new_form);

  MockPasswordStoreObserver mock_observer(store());
  password_manager::PasswordStoreChangeList list;
  list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE, old_form));
  EXPECT_CALL(mock_observer, OnLoginsChanged(list));
  if (check_created) {
    store()->RemoveLoginsCreatedBetween(base::Time(), next_day,
                                        base::Closure());
  } else {
    store()->RemoveLoginsSyncedBetween(base::Time(), next_day);
  }
  FinishAsyncProcessing();
}

TEST_F(PasswordStoreProxyMacTest, FormLifeCycle) {
  PasswordForm password_form;
  password_form.origin = GURL("http://example.com");
  password_form.username_value = base::ASCIIToUTF16("test1@gmail.com");
  password_form.password_value = base::ASCIIToUTF16("12345");
  password_form.signon_realm = "http://example.com/";

  AddForm(password_form);
  password_form.password_value = base::ASCIIToUTF16("password");
  UpdateForm(password_form);
  RemoveForm(password_form);
}

TEST_F(PasswordStoreProxyMacTest, TestRemoveLoginsCreatedBetween) {
  CheckRemoveLoginsBetween(true);
}

TEST_F(PasswordStoreProxyMacTest, TestRemoveLoginsSyncedBetween) {
  CheckRemoveLoginsBetween(false);
}

TEST_F(PasswordStoreProxyMacTest, FillLogins) {
  PasswordForm password_form;
  password_form.origin = GURL("http://example.com");
  password_form.signon_realm = "http://example.com/";
  password_form.username_value = base::ASCIIToUTF16("test1@gmail.com");
  password_form.password_value = base::ASCIIToUTF16("12345");
  AddForm(password_form);

  PasswordForm blacklisted_form;
  blacklisted_form.origin = GURL("http://example2.com");
  blacklisted_form.signon_realm = "http://example2.com/";
  blacklisted_form.blacklisted_by_user = true;
  AddForm(blacklisted_form);

  MockPasswordStoreConsumer mock_consumer;
  store()->GetLogins(password_form, PasswordStoreProxyMac::ALLOW_PROMPT,
                     &mock_consumer);
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(
                                 ElementsAre(Pointee(password_form))))
      .WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  store()->GetBlacklistLogins(&mock_consumer);
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(
                                 ElementsAre(Pointee(blacklisted_form))))
      .WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();

  store()->GetAutofillableLogins(&mock_consumer);
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(
                                 ElementsAre(Pointee(password_form))))
      .WillOnce(QuitUIMessageLoop());
  base::MessageLoop::current()->Run();
}

TEST_F(PasswordStoreProxyMacTest, OperationsOnABadDatabaseSilentlyFail) {
  // Verify that operations on a PasswordStore with a bad database cause no
  // explosions, but fail without side effect, return no data and trigger no
  // notifications.
  ClosePasswordStore();
  CreateAndInitPasswordStore(make_scoped_ptr(new BadLoginDatabase));
  FinishAsyncProcessing();
  EXPECT_FALSE(login_db());

  // The store should outlive the observer.
  scoped_refptr<PasswordStoreProxyMac> store_refptr = store();
  MockPasswordStoreObserver mock_observer(store());
  EXPECT_CALL(mock_observer, OnLoginsChanged(_)).Times(0);

  // Add a new autofillable login + a blacklisted login.
  password_manager::PasswordFormData www_form_data = {
      PasswordForm::SCHEME_HTML, "http://www.facebook.com/",
      "http://www.facebook.com/index.html", "login", L"username", L"password",
      L"submit", L"not_joe_user", L"12345", true, false, 1};
  scoped_ptr<PasswordForm> form =
      CreatePasswordFormFromDataForTesting(www_form_data);
  scoped_ptr<PasswordForm> blacklisted_form(new PasswordForm(*form));
  blacklisted_form->signon_realm = "http://foo.example.com";
  blacklisted_form->origin = GURL("http://foo.example.com/origin");
  blacklisted_form->action = GURL("http://foo.example.com/action");
  blacklisted_form->blacklisted_by_user = true;
  store()->AddLogin(*form);
  store()->AddLogin(*blacklisted_form);
  FinishAsyncProcessing();

  // Get all logins; autofillable logins; blacklisted logins.
  MockPasswordStoreConsumer mock_consumer;
  store()->GetLogins(*form, password_manager::PasswordStore::DISALLOW_PROMPT,
                     &mock_consumer);
  ON_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(_))
      .WillByDefault(QuitUIMessageLoop());
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));
  base::MessageLoop::current()->Run();

  store()->GetAutofillableLogins(&mock_consumer);
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));
  base::MessageLoop::current()->Run();

  store()->GetBlacklistLogins(&mock_consumer);
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));
  base::MessageLoop::current()->Run();

  // Report metrics.
  store()->ReportMetrics("Test Username", true);
  FinishAsyncProcessing();

  // Change the login.
  form->password_value = base::ASCIIToUTF16("a different password");
  store()->UpdateLogin(*form);
  FinishAsyncProcessing();

  // Delete one login; a range of logins.
  store()->RemoveLogin(*form);
  store()->RemoveLoginsCreatedBetween(base::Time(), base::Time::Max(),
                                      base::Closure());
  store()->RemoveLoginsSyncedBetween(base::Time(), base::Time::Max());
  FinishAsyncProcessing();

  // Verify no notifications are fired during shutdown either.
  ClosePasswordStore();
}
}  // namespace
