// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_store_x.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/password_manager/core/browser/password_form_data.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using content::BrowserThread;
using password_manager::ContainsAllPasswordForms;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using testing::_;
using testing::DoAll;
using testing::ElementsAreArray;
using testing::Pointee;
using testing::Property;
using testing::WithArg;

typedef std::vector<PasswordForm*> VectorOfForms;

namespace {

class MockPasswordStoreConsumer
    : public password_manager::PasswordStoreConsumer {
 public:
  MOCK_METHOD1(OnGetPasswordStoreResults,
               void(const std::vector<PasswordForm*>&));
};

class MockPasswordStoreObserver
    : public password_manager::PasswordStore::Observer {
 public:
  MOCK_METHOD1(OnLoginsChanged,
               void(const password_manager::PasswordStoreChangeList& changes));
};

class FailingBackend : public PasswordStoreX::NativeBackend {
 public:
  virtual bool Init() OVERRIDE { return true; }

  virtual PasswordStoreChangeList AddLogin(const PasswordForm& form) OVERRIDE {
    return PasswordStoreChangeList();
  }
  virtual bool UpdateLogin(const PasswordForm& form,
                           PasswordStoreChangeList* changes) OVERRIDE {
    return false;
  }
  virtual bool RemoveLogin(const PasswordForm& form) OVERRIDE { return false; }

  virtual bool RemoveLoginsCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) OVERRIDE {
    return false;
  }

  virtual bool RemoveLoginsSyncedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) OVERRIDE {
    return false;
  }

  virtual bool GetLogins(const PasswordForm& form,
                         PasswordFormList* forms) OVERRIDE {
    return false;
  }

  virtual bool GetAutofillableLogins(PasswordFormList* forms) OVERRIDE {
    return false;
  }
  virtual bool GetBlacklistLogins(PasswordFormList* forms) OVERRIDE {
    return false;
  }
};

class MockBackend : public PasswordStoreX::NativeBackend {
 public:
  virtual bool Init() OVERRIDE { return true; }

  virtual PasswordStoreChangeList AddLogin(const PasswordForm& form) OVERRIDE {
    all_forms_.push_back(form);
    PasswordStoreChange change(PasswordStoreChange::ADD, form);
    return PasswordStoreChangeList(1, change);
  }

  virtual bool UpdateLogin(const PasswordForm& form,
                           PasswordStoreChangeList* changes) OVERRIDE {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (CompareForms(all_forms_[i], form, true)) {
        all_forms_[i] = form;
        changes->push_back(PasswordStoreChange(PasswordStoreChange::UPDATE,
                                               form));
      }
    return true;
  }

  virtual bool RemoveLogin(const PasswordForm& form) OVERRIDE {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (CompareForms(all_forms_[i], form, false))
        erase(i--);
    return true;
  }

  virtual bool RemoveLoginsCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) OVERRIDE {
    for (size_t i = 0; i < all_forms_.size(); ++i) {
      if (delete_begin <= all_forms_[i].date_created &&
          (delete_end.is_null() || all_forms_[i].date_created < delete_end))
        erase(i--);
    }
    return true;
  }

  virtual bool RemoveLoginsSyncedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) OVERRIDE {
    DCHECK(changes);
    for (size_t i = 0; i < all_forms_.size(); ++i) {
      if (delete_begin <= all_forms_[i].date_synced &&
          (delete_end.is_null() || all_forms_[i].date_synced < delete_end)) {
        changes->push_back(password_manager::PasswordStoreChange(
            password_manager::PasswordStoreChange::REMOVE, all_forms_[i]));
        erase(i--);
      }
    }
    return true;
  }

  virtual bool GetLogins(const PasswordForm& form,
                         PasswordFormList* forms) OVERRIDE {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (all_forms_[i].signon_realm == form.signon_realm)
        forms->push_back(new PasswordForm(all_forms_[i]));
    return true;
  }

  virtual bool GetAutofillableLogins(PasswordFormList* forms) OVERRIDE {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (!all_forms_[i].blacklisted_by_user)
        forms->push_back(new PasswordForm(all_forms_[i]));
    return true;
  }

  virtual bool GetBlacklistLogins(PasswordFormList* forms) OVERRIDE {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (all_forms_[i].blacklisted_by_user)
        forms->push_back(new PasswordForm(all_forms_[i]));
    return true;
  }

 private:
  void erase(size_t index) {
    if (index < all_forms_.size() - 1)
      all_forms_[index] = all_forms_[all_forms_.size() - 1];
    all_forms_.pop_back();
  }

  bool CompareForms(const PasswordForm& a, const PasswordForm& b, bool update) {
    // An update check doesn't care about the submit element.
    if (!update && a.submit_element != b.submit_element)
      return false;
    return a.origin           == b.origin &&
           a.password_element == b.password_element &&
           a.signon_realm     == b.signon_realm &&
           a.username_element == b.username_element &&
           a.username_value   == b.username_value;
  }

  std::vector<PasswordForm> all_forms_;
};

class MockLoginDatabaseReturn {
 public:
  MOCK_METHOD1(OnLoginDatabaseQueryDone,
               void(const std::vector<PasswordForm*>&));
};

void LoginDatabaseQueryCallback(password_manager::LoginDatabase* login_db,
                                bool autofillable,
                                MockLoginDatabaseReturn* mock_return) {
  std::vector<PasswordForm*> forms;
  if (autofillable)
    login_db->GetAutofillableLogins(&forms);
  else
    login_db->GetBlacklistLogins(&forms);
  mock_return->OnLoginDatabaseQueryDone(forms);
}

// Generate |count| expected logins, either auto-fillable or blacklisted.
void InitExpectedForms(bool autofillable, size_t count, VectorOfForms* forms) {
  const char* domain = autofillable ? "example" : "blacklisted";
  for (size_t i = 0; i < count; ++i) {
    std::string realm = base::StringPrintf("http://%zu.%s.com", i, domain);
    std::string origin = base::StringPrintf("http://%zu.%s.com/origin",
                                            i, domain);
    std::string action = base::StringPrintf("http://%zu.%s.com/action",
                                            i, domain);
    password_manager::PasswordFormData data = {
        PasswordForm::SCHEME_HTML,
        realm.c_str(),
        origin.c_str(),
        action.c_str(),
        L"submit_element",
        L"username_element",
        L"password_element",
        autofillable ? L"username_value" : NULL,
        autofillable ? L"password_value" : NULL,
        autofillable,
        false,
        static_cast<double>(i + 1)};
    forms->push_back(CreatePasswordFormFromData(data));
  }
}

}  // anonymous namespace

enum BackendType {
  NO_BACKEND,
  FAILING_BACKEND,
  WORKING_BACKEND
};

class PasswordStoreXTest : public testing::TestWithParam<BackendType> {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    login_db_.reset(new password_manager::LoginDatabase());
    ASSERT_TRUE(login_db_->Init(temp_dir_.path().Append("login_test")));
  }

  virtual void TearDown() {
    base::RunLoop().RunUntilIdle();
  }

  PasswordStoreX::NativeBackend* GetBackend() {
    switch (GetParam()) {
      case FAILING_BACKEND:
        return new FailingBackend();
      case WORKING_BACKEND:
        return new MockBackend();
      default:
        return NULL;
    }
  }

  content::TestBrowserThreadBundle thread_bundle_;

  scoped_ptr<password_manager::LoginDatabase> login_db_;
  base::ScopedTempDir temp_dir_;
};

ACTION(STLDeleteElements0) {
  STLDeleteContainerPointers(arg0.begin(), arg0.end());
}

TEST_P(PasswordStoreXTest, Notifications) {
  scoped_refptr<PasswordStoreX> store(
      new PasswordStoreX(base::MessageLoopProxy::current(),
                         base::MessageLoopProxy::current(),
                         login_db_.release(),
                         GetBackend()));
  store->Init(syncer::SyncableService::StartSyncFlare(), "");

  password_manager::PasswordFormData form_data = {
      PasswordForm::SCHEME_HTML,       "http://bar.example.com",
      "http://bar.example.com/origin", "http://bar.example.com/action",
      L"submit_element",               L"username_element",
      L"password_element",             L"username_value",
      L"password_value",               true,
      false,                           1};
  scoped_ptr<PasswordForm> form(CreatePasswordFormFromData(form_data));

  MockPasswordStoreObserver observer;
  store->AddObserver(&observer);

  const PasswordStoreChange expected_add_changes[] = {
    PasswordStoreChange(PasswordStoreChange::ADD, *form),
  };

  EXPECT_CALL(
      observer,
      OnLoginsChanged(ElementsAreArray(expected_add_changes)));

  // Adding a login should trigger a notification.
  store->AddLogin(*form);

  // The PasswordStore schedules tasks to run on the DB thread. Wait for them
  // to complete.
  base::RunLoop().RunUntilIdle();

  // Change the password.
  form->password_value = base::ASCIIToUTF16("a different password");

  const PasswordStoreChange expected_update_changes[] = {
    PasswordStoreChange(PasswordStoreChange::UPDATE, *form),
  };

  EXPECT_CALL(
      observer,
      OnLoginsChanged(ElementsAreArray(expected_update_changes)));

  // Updating the login with the new password should trigger a notification.
  store->UpdateLogin(*form);

  // Wait for PasswordStore to send execute.
  base::RunLoop().RunUntilIdle();

  const PasswordStoreChange expected_delete_changes[] = {
    PasswordStoreChange(PasswordStoreChange::REMOVE, *form),
  };

  EXPECT_CALL(
      observer,
      OnLoginsChanged(ElementsAreArray(expected_delete_changes)));

  // Deleting the login should trigger a notification.
  store->RemoveLogin(*form);

  // Wait for PasswordStore to execute.
  base::RunLoop().RunUntilIdle();

  store->RemoveObserver(&observer);

  store->Shutdown();
}

TEST_P(PasswordStoreXTest, NativeMigration) {
  VectorOfForms expected_autofillable;
  InitExpectedForms(true, 50, &expected_autofillable);

  VectorOfForms expected_blacklisted;
  InitExpectedForms(false, 50, &expected_blacklisted);

  // Get the initial size of the login DB file, before we populate it.
  // This will be used later to make sure it gets back to this size.
  const base::FilePath login_db_file = temp_dir_.path().Append("login_test");
  base::File::Info db_file_start_info;
  ASSERT_TRUE(base::GetFileInfo(login_db_file, &db_file_start_info));

  password_manager::LoginDatabase* login_db = login_db_.get();

  // Populate the login DB with logins that should be migrated.
  for (VectorOfForms::iterator it = expected_autofillable.begin();
       it != expected_autofillable.end(); ++it) {
    login_db->AddLogin(**it);
  }
  for (VectorOfForms::iterator it = expected_blacklisted.begin();
       it != expected_blacklisted.end(); ++it) {
    login_db->AddLogin(**it);
  }

  // Get the new size of the login DB file. We expect it to be larger.
  base::File::Info db_file_full_info;
  ASSERT_TRUE(base::GetFileInfo(login_db_file, &db_file_full_info));
  EXPECT_GT(db_file_full_info.size, db_file_start_info.size);

  // Initializing the PasswordStore shouldn't trigger a native migration (yet).
  scoped_refptr<PasswordStoreX> store(
      new PasswordStoreX(base::MessageLoopProxy::current(),
                         base::MessageLoopProxy::current(),
                         login_db_.release(),
                         GetBackend()));
  store->Init(syncer::SyncableService::StartSyncFlare(), "");

  MockPasswordStoreConsumer consumer;

  // The autofillable forms should have been migrated to the native backend.
  EXPECT_CALL(consumer,
      OnGetPasswordStoreResults(
          ContainsAllPasswordForms(expected_autofillable)))
      .WillOnce(WithArg<0>(STLDeleteElements0()));

  store->GetAutofillableLogins(&consumer);
  base::RunLoop().RunUntilIdle();

  // The blacklisted forms should have been migrated to the native backend.
  EXPECT_CALL(consumer,
      OnGetPasswordStoreResults(ContainsAllPasswordForms(expected_blacklisted)))
      .WillOnce(WithArg<0>(STLDeleteElements0()));

  store->GetBlacklistLogins(&consumer);
  base::RunLoop().RunUntilIdle();

  VectorOfForms empty;
  MockLoginDatabaseReturn ld_return;

  if (GetParam() == WORKING_BACKEND) {
    // No autofillable logins should be left in the login DB.
    EXPECT_CALL(ld_return,
                OnLoginDatabaseQueryDone(ContainsAllPasswordForms(empty)));
  } else {
    // The autofillable logins should still be in the login DB.
    EXPECT_CALL(ld_return,
                OnLoginDatabaseQueryDone(
                    ContainsAllPasswordForms(expected_autofillable)))
        .WillOnce(WithArg<0>(STLDeleteElements0()));
  }

  LoginDatabaseQueryCallback(login_db, true, &ld_return);

  // Wait for the login DB methods to execute.
  base::RunLoop().RunUntilIdle();

  if (GetParam() == WORKING_BACKEND) {
    // Likewise, no blacklisted logins should be left in the login DB.
    EXPECT_CALL(ld_return,
                OnLoginDatabaseQueryDone(ContainsAllPasswordForms(empty)));
  } else {
    // The blacklisted logins should still be in the login DB.
    EXPECT_CALL(ld_return,
                OnLoginDatabaseQueryDone(
                    ContainsAllPasswordForms(expected_blacklisted)))
        .WillOnce(WithArg<0>(STLDeleteElements0()));
  }

  LoginDatabaseQueryCallback(login_db, false, &ld_return);

  // Wait for the login DB methods to execute.
  base::RunLoop().RunUntilIdle();

  if (GetParam() == WORKING_BACKEND) {
    // If the migration succeeded, then not only should there be no logins left
    // in the login DB, but also the file should have been deleted and then
    // recreated. We approximate checking for this by checking that the file
    // size is equal to the size before we populated it, even though it was
    // larger after populating it.
    base::File::Info db_file_end_info;
    ASSERT_TRUE(base::GetFileInfo(login_db_file, &db_file_end_info));
    EXPECT_EQ(db_file_start_info.size, db_file_end_info.size);
  }

  STLDeleteElements(&expected_autofillable);
  STLDeleteElements(&expected_blacklisted);

  store->Shutdown();
}

INSTANTIATE_TEST_CASE_P(NoBackend,
                        PasswordStoreXTest,
                        testing::Values(NO_BACKEND));
INSTANTIATE_TEST_CASE_P(FailingBackend,
                        PasswordStoreXTest,
                        testing::Values(FAILING_BACKEND));
INSTANTIATE_TEST_CASE_P(WorkingBackend,
                        PasswordStoreXTest,
                        testing::Values(WORKING_BACKEND));
