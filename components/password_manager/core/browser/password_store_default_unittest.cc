// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using testing::ElementsAreArray;
using testing::IsEmpty;

namespace password_manager {

namespace {

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MOCK_METHOD1(OnGetPasswordStoreResultsConstRef,
               void(const std::vector<PasswordForm*>&));

  // GMock cannot mock methods with move-only args.
  void OnGetPasswordStoreResults(ScopedVector<PasswordForm> results) override {
    OnGetPasswordStoreResultsConstRef(results.get());
  }
};

class MockPasswordStoreObserver : public PasswordStore::Observer {
 public:
  MOCK_METHOD1(OnLoginsChanged, void(const PasswordStoreChangeList& changes));
};

// A mock LoginDatabase that simulates a failing Init() method.
class BadLoginDatabase : public LoginDatabase {
 public:
  BadLoginDatabase() : LoginDatabase(base::FilePath()) {}
  ~BadLoginDatabase() override {}

  // LoginDatabase:
  bool Init() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(BadLoginDatabase);
};

PasswordFormData CreateTestPasswordFormData() {
  PasswordFormData data = {
    PasswordForm::SCHEME_HTML,
    "http://bar.example.com",
    "http://bar.example.com/origin",
    "http://bar.example.com/action",
    L"submit_element",
    L"username_element",
    L"password_element",
    L"username_value",
    L"password_value",
    true,
    false,
    1
  };
  return data;
}

}  // anonymous namespace

class PasswordStoreDefaultTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override { ASSERT_TRUE(temp_dir_.Delete()); }

  base::FilePath test_login_db_file_path() const {
    return temp_dir_.path().Append(FILE_PATH_LITERAL("login_test"));
  }

  base::MessageLoopForUI message_loop_;
  base::ScopedTempDir temp_dir_;
};

ACTION(STLDeleteElements0) {
  STLDeleteContainerPointers(arg0.begin(), arg0.end());
}

TEST_F(PasswordStoreDefaultTest, NonASCIIData) {
  scoped_refptr<PasswordStoreDefault> store(new PasswordStoreDefault(
      base::MessageLoopProxy::current(), base::MessageLoopProxy::current(),
      make_scoped_ptr(new LoginDatabase(test_login_db_file_path()))));
  store->Init(syncer::SyncableService::StartSyncFlare());

  // Some non-ASCII password form data.
  static const PasswordFormData form_data[] = {
    { PasswordForm::SCHEME_HTML,
      "http://foo.example.com",
      "http://foo.example.com/origin",
      "http://foo.example.com/action",
      L"มีสีสัน",
      L"お元気ですか?",
      L"盆栽",
      L"أحب كرة",
      L"£éä국수çà",
      true, false, 1 },
  };

  // Build the expected forms vector and add the forms to the store.
  ScopedVector<PasswordForm> expected_forms;
  for (unsigned int i = 0; i < arraysize(form_data); ++i) {
    expected_forms.push_back(
        CreatePasswordFormFromDataForTesting(form_data[i]).release());
    store->AddLogin(*expected_forms.back());
  }

  base::MessageLoop::current()->RunUntilIdle();

  MockPasswordStoreConsumer consumer;

  // We expect to get the same data back, even though it's not all ASCII.
  EXPECT_CALL(consumer, OnGetPasswordStoreResultsConstRef(
                            ContainsSamePasswordForms(expected_forms.get())));
  store->GetAutofillableLogins(&consumer);

  base::MessageLoop::current()->RunUntilIdle();

  store->Shutdown();
  base::MessageLoop::current()->RunUntilIdle();
}

TEST_F(PasswordStoreDefaultTest, Notifications) {
  scoped_refptr<PasswordStoreDefault> store(new PasswordStoreDefault(
      base::MessageLoopProxy::current(), base::MessageLoopProxy::current(),
      make_scoped_ptr(new LoginDatabase(test_login_db_file_path()))));
  store->Init(syncer::SyncableService::StartSyncFlare());

  scoped_ptr<PasswordForm> form =
      CreatePasswordFormFromDataForTesting(CreateTestPasswordFormData());

  MockPasswordStoreObserver observer;
  store->AddObserver(&observer);

  const PasswordStoreChange expected_add_changes[] = {
    PasswordStoreChange(PasswordStoreChange::ADD, *form),
  };

  EXPECT_CALL(observer,
              OnLoginsChanged(ElementsAreArray(expected_add_changes)));

  // Adding a login should trigger a notification.
  store->AddLogin(*form);
  base::MessageLoop::current()->RunUntilIdle();

  // Change the password.
  form->password_value = base::ASCIIToUTF16("a different password");

  const PasswordStoreChange expected_update_changes[] = {
    PasswordStoreChange(PasswordStoreChange::UPDATE, *form),
  };

  EXPECT_CALL(observer,
              OnLoginsChanged(ElementsAreArray(expected_update_changes)));

  // Updating the login with the new password should trigger a notification.
  store->UpdateLogin(*form);
  base::MessageLoop::current()->RunUntilIdle();

  const PasswordStoreChange expected_delete_changes[] = {
    PasswordStoreChange(PasswordStoreChange::REMOVE, *form),
  };

  EXPECT_CALL(observer,
              OnLoginsChanged(ElementsAreArray(expected_delete_changes)));

  // Deleting the login should trigger a notification.
  store->RemoveLogin(*form);
  base::MessageLoop::current()->RunUntilIdle();

  store->RemoveObserver(&observer);
  store->Shutdown();
  base::MessageLoop::current()->RunUntilIdle();
}

// Verify that operations on a PasswordStore with a bad database cause no
// explosions, but fail without side effect, return no data and trigger no
// notifications.
TEST_F(PasswordStoreDefaultTest, OperationsOnABadDatabaseSilentlyFail) {
  scoped_refptr<PasswordStoreDefault> bad_store(new PasswordStoreDefault(
      base::MessageLoopProxy::current(), base::MessageLoopProxy::current(),
      make_scoped_ptr<LoginDatabase>(new BadLoginDatabase)));

  bad_store->Init(syncer::SyncableService::StartSyncFlare());
  base::MessageLoop::current()->RunUntilIdle();
  ASSERT_EQ(nullptr, bad_store->login_db());

  testing::StrictMock<MockPasswordStoreObserver> mock_observer;
  bad_store->AddObserver(&mock_observer);

  // Add a new autofillable login + a blacklisted login.
  scoped_ptr<PasswordForm> form =
      CreatePasswordFormFromDataForTesting(CreateTestPasswordFormData());
  scoped_ptr<PasswordForm> blacklisted_form(new PasswordForm(*form));
  blacklisted_form->signon_realm = "http://foo.example.com";
  blacklisted_form->origin = GURL("http://foo.example.com/origin");
  blacklisted_form->action = GURL("http://foo.example.com/action");
  blacklisted_form->blacklisted_by_user = true;
  bad_store->AddLogin(*form);
  bad_store->AddLogin(*blacklisted_form);
  base::MessageLoop::current()->RunUntilIdle();

  // Get all logins; autofillable logins; blacklisted logins.
  testing::StrictMock<MockPasswordStoreConsumer> mock_consumer;
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));
  bad_store->GetLogins(*form, PasswordStore::DISALLOW_PROMPT, &mock_consumer);
  base::MessageLoop::current()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_consumer);
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));
  bad_store->GetAutofillableLogins(&mock_consumer);
  base::MessageLoop::current()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_consumer);
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));
  bad_store->GetBlacklistLogins(&mock_consumer);
  base::MessageLoop::current()->RunUntilIdle();

  // Report metrics.
  bad_store->ReportMetrics("Test Username", true);
  base::MessageLoop::current()->RunUntilIdle();

  // Change the login.
  form->password_value = base::ASCIIToUTF16("a different password");
  bad_store->UpdateLogin(*form);
  base::MessageLoop::current()->RunUntilIdle();

  // Delete one login; a range of logins.
  bad_store->RemoveLogin(*form);
  base::MessageLoop::current()->RunUntilIdle();
  bad_store->RemoveLoginsCreatedBetween(base::Time(), base::Time::Max());
  base::MessageLoop::current()->RunUntilIdle();
  bad_store->RemoveLoginsSyncedBetween(base::Time(), base::Time::Max());
  base::MessageLoop::current()->RunUntilIdle();

  // Ensure no notifications and no explosions during shutdown either.
  bad_store->RemoveObserver(&mock_observer);
  bad_store->Shutdown();
  base::MessageLoop::current()->RunUntilIdle();
}

}  // namespace password_manager
