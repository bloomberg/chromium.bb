// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_form_data.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::WaitableEvent;
using testing::_;
using testing::DoAll;
using testing::ElementsAreArray;
using testing::Pointee;
using testing::Property;
using testing::WithArg;

namespace password_manager {

namespace {

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MOCK_METHOD1(OnGetPasswordStoreResults,
               void(const std::vector<PasswordForm*>&));
};

class MockPasswordStoreObserver : public PasswordStore::Observer {
 public:
  MOCK_METHOD1(OnLoginsChanged,
               void(const PasswordStoreChangeList& changes));
};

}  // anonymous namespace

class PasswordStoreDefaultTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    login_db_.reset(new LoginDatabase());
    ASSERT_TRUE(login_db_->Init(temp_dir_.path().Append(
        FILE_PATH_LITERAL("login_test"))));
  }

  virtual void TearDown() OVERRIDE {
    ASSERT_TRUE(temp_dir_.Delete());
  }

  base::MessageLoopForUI message_loop_;
  scoped_ptr<LoginDatabase> login_db_;
  base::ScopedTempDir temp_dir_;
};

ACTION(STLDeleteElements0) {
  STLDeleteContainerPointers(arg0.begin(), arg0.end());
}

TEST_F(PasswordStoreDefaultTest, NonASCIIData) {
  scoped_refptr<PasswordStoreDefault> store(new PasswordStoreDefault(
      base::MessageLoopProxy::current(),
      base::MessageLoopProxy::current(),
      login_db_.release()));
  store->Init(syncer::SyncableService::StartSyncFlare(), "");

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
  std::vector<PasswordForm*> expected_forms;
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(form_data); ++i) {
    PasswordForm* form = CreatePasswordFormFromData(form_data[i]);
    expected_forms.push_back(form);
    store->AddLogin(*form);
  }

  base::MessageLoop::current()->RunUntilIdle();

  MockPasswordStoreConsumer consumer;

  // We expect to get the same data back, even though it's not all ASCII.
  EXPECT_CALL(consumer,
      OnGetPasswordStoreResults(ContainsAllPasswordForms(expected_forms)))
      .WillOnce(WithArg<0>(STLDeleteElements0()));
  store->GetAutofillableLogins(&consumer);

  base::MessageLoop::current()->RunUntilIdle();

  STLDeleteElements(&expected_forms);
  store->Shutdown();
  base::MessageLoop::current()->RunUntilIdle();
}

TEST_F(PasswordStoreDefaultTest, Notifications) {
  scoped_refptr<PasswordStoreDefault> store(new PasswordStoreDefault(
      base::MessageLoopProxy::current(),
      base::MessageLoopProxy::current(),
      login_db_.release()));
  store->Init(syncer::SyncableService::StartSyncFlare(), "");

  PasswordFormData form_data =
  { PasswordForm::SCHEME_HTML,
    "http://bar.example.com",
    "http://bar.example.com/origin",
    "http://bar.example.com/action",
    L"submit_element",
    L"username_element",
    L"password_element",
    L"username_value",
    L"password_value",
    true, false, 1 };
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
  base::MessageLoop::current()->RunUntilIdle();

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
  base::MessageLoop::current()->RunUntilIdle();

  const PasswordStoreChange expected_delete_changes[] = {
    PasswordStoreChange(PasswordStoreChange::REMOVE, *form),
  };

  EXPECT_CALL(
      observer,
      OnLoginsChanged(ElementsAreArray(expected_delete_changes)));

  // Deleting the login should trigger a notification.
  store->RemoveLogin(*form);
  base::MessageLoop::current()->RunUntilIdle();

  store->RemoveObserver(&observer);
  store->Shutdown();
  base::MessageLoop::current()->RunUntilIdle();
}

}  // namespace password_manager
