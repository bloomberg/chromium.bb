// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_proxy_mac.h"

#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/password_manager/password_store_mac.h"
#include "chrome/browser/password_manager/password_store_mac_internal.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "crypto/mock_apple_keychain.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {

using autofill::PasswordForm;
using content::BrowserThread;
using password_manager::MigrationStatus;
using password_manager::PasswordStore;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pointee;

// Returns a change list corresponding to |form| being added.
PasswordStoreChangeList AddChangeForForm(const PasswordForm& form) {
  return PasswordStoreChangeList(
      1, PasswordStoreChange(PasswordStoreChange::ADD, form));
}

class MockPasswordStoreConsumer
    : public password_manager::PasswordStoreConsumer {
 public:
  MockPasswordStoreConsumer() = default;

  void WaitForResult() {
    base::RunLoop run_loop;
    nested_loop_ = &run_loop;
    run_loop.Run();
    nested_loop_ = nullptr;
  }

  const std::vector<std::unique_ptr<PasswordForm>>& forms() const {
    return forms_;
  }

 private:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    forms_.swap(results);
    if (nested_loop_)
      nested_loop_->Quit();
  }

  std::vector<std::unique_ptr<PasswordForm>> forms_;
  base::RunLoop* nested_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockPasswordStoreConsumer);
};

class MockPasswordStoreObserver
    : public password_manager::PasswordStore::Observer {
 public:
  explicit MockPasswordStoreObserver(PasswordStoreProxyMac* password_store)
      : guard_(this) {
    guard_.Add(password_store);
  }
  MOCK_METHOD1(OnLoginsChanged,
               void(const password_manager::PasswordStoreChangeList& changes));

 private:
  ScopedObserver<PasswordStoreProxyMac, MockPasswordStoreObserver> guard_;

  DISALLOW_COPY_AND_ASSIGN(MockPasswordStoreObserver);
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

class PasswordStoreProxyMacTest
    : public testing::TestWithParam<MigrationStatus> {
 public:
  PasswordStoreProxyMacTest();
  ~PasswordStoreProxyMacTest() override;

  void SetUp() override;
  void TearDown() override;

  void CreateAndInitPasswordStore(
      std::unique_ptr<password_manager::LoginDatabase> login_db);

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

  // Returns the expected migration status after the password store was inited.
  MigrationStatus GetTargetStatus(bool keychain_locked) const;

  password_manager::LoginDatabase* login_db() const {
    return store_->login_metadata_db();
  }

  PasswordStoreProxyMac* store() { return store_.get(); }

 protected:
  content::TestBrowserThreadBundle ui_thread_;

  base::ScopedTempDir db_dir_;
  scoped_refptr<PasswordStoreProxyMac> store_;
  sync_preferences::TestingPrefServiceSyncable testing_prefs_;
};

PasswordStoreProxyMacTest::PasswordStoreProxyMacTest() {
  EXPECT_TRUE(db_dir_.CreateUniqueTempDir());
  chrome::RegisterUserProfilePrefs(testing_prefs_.registry());
  testing_prefs_.SetInteger(password_manager::prefs::kKeychainMigrationStatus,
                            static_cast<int>(GetParam()));
  // Ensure that LoginDatabase will use the mock keychain if it needs to
  // encrypt/decrypt a password.
  OSCryptMocker::SetUpWithSingleton();
}

PasswordStoreProxyMacTest::~PasswordStoreProxyMacTest() {}

void PasswordStoreProxyMacTest::SetUp() {
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      new password_manager::LoginDatabase(test_login_db_file_path()));
  CreateAndInitPasswordStore(std::move(login_db));
}

void PasswordStoreProxyMacTest::TearDown() {
  ClosePasswordStore();
  OSCryptMocker::TearDown();
}

void PasswordStoreProxyMacTest::CreateAndInitPasswordStore(
    std::unique_ptr<password_manager::LoginDatabase> login_db) {
  store_ = new PasswordStoreProxyMac(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
      base::MakeUnique<crypto::MockAppleKeychain>(), std::move(login_db),
      &testing_prefs_);
  ASSERT_TRUE(store_->Init(syncer::SyncableService::StartSyncFlare()));
}

void PasswordStoreProxyMacTest::ClosePasswordStore() {
  if (!store_)
    return;
  store_->ShutdownOnUIThread();
  EXPECT_FALSE(store_->GetBackgroundTaskRunner());
  store_ = nullptr;
}

void PasswordStoreProxyMacTest::FinishAsyncProcessing() {
  // Do a store-level query to wait for all the previously enqueued operations
  // to finish.
  MockPasswordStoreConsumer consumer;
  store_->GetLogins({PasswordForm::SCHEME_HTML, std::string(), GURL()},
                    &consumer);
  consumer.WaitForResult();
}

base::FilePath PasswordStoreProxyMacTest::test_login_db_file_path() const {
  return db_dir_.GetPath().Append(FILE_PATH_LITERAL("login.db"));
}

MigrationStatus PasswordStoreProxyMacTest::GetTargetStatus(
    bool keychain_locked) const {
  if (GetParam() == MigrationStatus::NOT_STARTED ||
      GetParam() == MigrationStatus::FAILED_ONCE ||
      GetParam() == MigrationStatus::FAILED_TWICE) {
    return MigrationStatus::MIGRATION_STOPPED;
  }
  if (GetParam() == MigrationStatus::MIGRATED)
    return MigrationStatus::MIGRATED_DELETED;
  return GetParam();
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
  old_form.federation_origin =
      url::Origin(GURL("http://accounts.google.com/federation"));

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

// ----------- Tests -------------

TEST_P(PasswordStoreProxyMacTest, StartAndStop) {
  // PasswordStore::ShutdownOnUIThread() immediately follows
  // PasswordStore::Init(). The message loop isn't running in between. Anyway,
  // PasswordStore should end up in the right state.
  ClosePasswordStore();

  int status = testing_prefs_.GetInteger(
      password_manager::prefs::kKeychainMigrationStatus);
  EXPECT_EQ(static_cast<int>(GetTargetStatus(false)), status);
}

TEST_P(PasswordStoreProxyMacTest, FormLifeCycle) {
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

TEST_P(PasswordStoreProxyMacTest, TestRemoveLoginsCreatedBetween) {
  CheckRemoveLoginsBetween(true);
}

TEST_P(PasswordStoreProxyMacTest, TestRemoveLoginsSyncedBetween) {
  CheckRemoveLoginsBetween(false);
}

TEST_P(PasswordStoreProxyMacTest, FillLogins) {
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
  store()->GetLogins(PasswordStore::FormDigest(password_form), &mock_consumer);
  mock_consumer.WaitForResult();
  EXPECT_THAT(mock_consumer.forms(), ElementsAre(Pointee(password_form)));

  store()->GetBlacklistLogins(&mock_consumer);
  mock_consumer.WaitForResult();
  EXPECT_THAT(mock_consumer.forms(), ElementsAre(Pointee(blacklisted_form)));

  store()->GetAutofillableLogins(&mock_consumer);
  mock_consumer.WaitForResult();
  EXPECT_THAT(mock_consumer.forms(), ElementsAre(Pointee(password_form)));
}

TEST_P(PasswordStoreProxyMacTest, OperationsOnABadDatabaseSilentlyFail) {
  // Verify that operations on a PasswordStore with a bad database cause no
  // explosions, but fail without side effect, return no data and trigger no
  // notifications.
  ClosePasswordStore();
  CreateAndInitPasswordStore(base::MakeUnique<BadLoginDatabase>());
  FinishAsyncProcessing();
  EXPECT_FALSE(login_db());

  // The store should outlive the observer.
  scoped_refptr<PasswordStoreProxyMac> store_refptr = store();
  MockPasswordStoreObserver mock_observer(store());
  EXPECT_CALL(mock_observer, OnLoginsChanged(_)).Times(0);

  // Add a new autofillable login + a blacklisted login.
  password_manager::PasswordFormData www_form_data = {
      PasswordForm::SCHEME_HTML,
      "http://www.facebook.com/",
      "http://www.facebook.com/index.html",
      "login",
      L"username",
      L"password",
      L"submit",
      L"not_joe_user",
      L"12345",
      true,
      1};
  std::unique_ptr<PasswordForm> form =
      CreatePasswordFormFromDataForTesting(www_form_data);
  std::unique_ptr<PasswordForm> blacklisted_form(new PasswordForm(*form));
  blacklisted_form->signon_realm = "http://foo.example.com";
  blacklisted_form->origin = GURL("http://foo.example.com/origin");
  blacklisted_form->action = GURL("http://foo.example.com/action");
  blacklisted_form->blacklisted_by_user = true;
  store()->AddLogin(*form);
  store()->AddLogin(*blacklisted_form);
  FinishAsyncProcessing();

  // Get all logins; autofillable logins; blacklisted logins.
  MockPasswordStoreConsumer mock_consumer;
  store()->GetLogins(PasswordStore::FormDigest(*form), &mock_consumer);
  mock_consumer.WaitForResult();
  EXPECT_THAT(mock_consumer.forms(), IsEmpty());

  store()->GetAutofillableLogins(&mock_consumer);
  mock_consumer.WaitForResult();
  EXPECT_THAT(mock_consumer.forms(), IsEmpty());

  store()->GetBlacklistLogins(&mock_consumer);
  mock_consumer.WaitForResult();
  EXPECT_THAT(mock_consumer.forms(), IsEmpty());

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

INSTANTIATE_TEST_CASE_P(,
                        PasswordStoreProxyMacTest,
                        testing::Values(MigrationStatus::NOT_STARTED,
                                        MigrationStatus::MIGRATED,
                                        MigrationStatus::FAILED_ONCE,
                                        MigrationStatus::FAILED_TWICE,
                                        MigrationStatus::MIGRATED_DELETED,
                                        MigrationStatus::MIGRATED_PARTIALLY,
                                        MigrationStatus::MIGRATION_STOPPED));

// Test the migration process.
class PasswordStoreProxyMacMigrationTest : public PasswordStoreProxyMacTest {
 public:
  void SetUp() override;

  void TestMigration(bool lock_keychain);

 protected:
  std::unique_ptr<password_manager::LoginDatabase> login_db_;
  std::unique_ptr<crypto::MockAppleKeychain> keychain_;
  base::HistogramTester histogram_tester_;
};

void PasswordStoreProxyMacMigrationTest::SetUp() {
  login_db_.reset(
      new password_manager::LoginDatabase(test_login_db_file_path()));
  keychain_.reset(new crypto::MockAppleKeychain);
}

void PasswordStoreProxyMacMigrationTest::TestMigration(bool lock_keychain) {
  PasswordForm form;
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = base::ASCIIToUTF16("my_username");
  form.password_value = base::ASCIIToUTF16("12345");

  const bool before_migration = (GetParam() == MigrationStatus::NOT_STARTED ||
                                  GetParam() == MigrationStatus::FAILED_ONCE ||
                                  GetParam() == MigrationStatus::FAILED_TWICE);
  if (before_migration)
    login_db_->set_clear_password_values(true);
  EXPECT_TRUE(login_db_->Init());
  EXPECT_EQ(AddChangeForForm(form), login_db_->AddLogin(form));
  // Prepare another database instance with the same content which is to be
  // initialized by PasswordStoreProxyMac.
  login_db_.reset(
      new password_manager::LoginDatabase(test_login_db_file_path()));
  MacKeychainPasswordFormAdapter adapter(keychain_.get());
  EXPECT_TRUE(adapter.AddPassword(form));

  // Init the store. It may trigger the migration.
  if (lock_keychain)
    keychain_->set_locked(true);
  store_ = new PasswordStoreProxyMac(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
      std::move(keychain_), std::move(login_db_), &testing_prefs_);
  ASSERT_TRUE(store_->Init(syncer::SyncableService::StartSyncFlare()));
  FinishAsyncProcessing();

  MockPasswordStoreConsumer mock_consumer;
  store()->GetLogins(PasswordStore::FormDigest(form), &mock_consumer);
  mock_consumer.WaitForResult();
  if (before_migration)
    EXPECT_THAT(mock_consumer.forms(), IsEmpty());
  else
    EXPECT_THAT(mock_consumer.forms(), ElementsAre(Pointee(form)));

  int status = testing_prefs_.GetInteger(
      password_manager::prefs::kKeychainMigrationStatus);
  EXPECT_EQ(static_cast<int>(GetTargetStatus(lock_keychain)), status);
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.KeychainMigration.Status", status, 1);
  static_cast<crypto::MockAppleKeychain*>(store()->keychain())
      ->set_locked(false);
  if (GetTargetStatus(lock_keychain) == MigrationStatus::MIGRATED_DELETED &&
      GetParam() != MigrationStatus::MIGRATED_DELETED && !lock_keychain) {
    EXPECT_THAT(adapter.GetAllPasswordFormPasswords(), IsEmpty());
  } else {
    auto forms = adapter.GetAllPasswordFormPasswords();
    ASSERT_EQ(1u, forms.size());
    form.date_created = forms[0]->date_created;
    EXPECT_THAT(adapter.GetAllPasswordFormPasswords(),
                ElementsAre(Pointee(form)));
  }
}

TEST_P(PasswordStoreProxyMacMigrationTest, TestSuccessfullMigration) {
  TestMigration(false);
}

TEST_P(PasswordStoreProxyMacMigrationTest, TestFailedMigration) {
  TestMigration(true);
}

INSTANTIATE_TEST_CASE_P(,
                        PasswordStoreProxyMacMigrationTest,
                        testing::Values(MigrationStatus::NOT_STARTED,
                                        MigrationStatus::MIGRATED,
                                        MigrationStatus::FAILED_ONCE,
                                        MigrationStatus::FAILED_TWICE,
                                        MigrationStatus::MIGRATED_DELETED,
                                        MigrationStatus::MIGRATED_PARTIALLY,
                                        MigrationStatus::MIGRATION_STOPPED));

}  // namespace
