// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::WaitableEvent;
using testing::_;
using testing::DoAll;
using testing::WithArg;

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

class StartSyncFlareMock {
 public:
  StartSyncFlareMock() {}
  ~StartSyncFlareMock() {}

  MOCK_METHOD1(StartSyncFlare, void(syncer::ModelType));
};

}  // namespace

class PasswordStoreTest : public testing::Test {
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

TEST_F(PasswordStoreTest, IgnoreOldWwwGoogleLogins) {
  scoped_refptr<PasswordStoreDefault> store(new PasswordStoreDefault(
      base::MessageLoopProxy::current(), base::MessageLoopProxy::current(),
      make_scoped_ptr(new LoginDatabase(test_login_db_file_path()))));
  store->Init(syncer::SyncableService::StartSyncFlare());

  const time_t cutoff = 1325376000;  // 00:00 Jan 1 2012 UTC
  // The passwords are all empty because PasswordStoreDefault doesn't store the
  // actual passwords on OS X (they're stored in the Keychain instead). We could
  // special-case it, but it's easier to just have empty passwords.
  static const PasswordFormData form_data[] = {
    // A form on https://www.google.com/ older than the cutoff. Will be ignored.
    { PasswordForm::SCHEME_HTML,
      "https://www.google.com",
      "https://www.google.com/origin",
      "https://www.google.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value_1",
      L"",
      true, true, cutoff - 1 },
    // A form on https://www.google.com/ older than the cutoff. Will be ignored.
    { PasswordForm::SCHEME_HTML,
      "https://www.google.com",
      "https://www.google.com/origin",
      "https://www.google.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value_2",
      L"",
      true, true, cutoff - 1 },
    // A form on https://www.google.com/ newer than the cutoff.
    { PasswordForm::SCHEME_HTML,
      "https://www.google.com",
      "https://www.google.com/origin",
      "https://www.google.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value_3",
      L"",
      true, true, cutoff + 1 },
    // A form on https://accounts.google.com/ older than the cutoff.
    { PasswordForm::SCHEME_HTML,
      "https://accounts.google.com",
      "https://accounts.google.com/origin",
      "https://accounts.google.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value",
      L"",
      true, true, cutoff - 1 },
    // A form on http://bar.example.com/ older than the cutoff.
    { PasswordForm::SCHEME_HTML,
      "http://bar.example.com",
      "http://bar.example.com/origin",
      "http://bar.example.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value",
      L"",
      true, false, cutoff - 1 },
  };

  // Build the forms vector and add the forms to the store.
  ScopedVector<PasswordForm> all_forms;
  for (size_t i = 0; i < arraysize(form_data); ++i) {
    all_forms.push_back(
        CreatePasswordFormFromDataForTesting(form_data[i]).release());
    store->AddLogin(*all_forms.back());
  }
  base::MessageLoop::current()->RunUntilIdle();

  // We expect to get back only the "recent" www.google.com login.
  // Theoretically these should never actually exist since there are no longer
  // any login forms on www.google.com to save, but we technically allow them.
  // We should not get back the older saved password though.
  PasswordForm www_google;
  www_google.scheme = PasswordForm::SCHEME_HTML;
  www_google.signon_realm = "https://www.google.com";
  std::vector<PasswordForm*> www_google_expected;
  www_google_expected.push_back(all_forms[2]);

  // We should still get the accounts.google.com login even though it's older
  // than our cutoff - this is the new location of all Google login forms.
  PasswordForm accounts_google;
  accounts_google.scheme = PasswordForm::SCHEME_HTML;
  accounts_google.signon_realm = "https://accounts.google.com";
  std::vector<PasswordForm*> accounts_google_expected;
  accounts_google_expected.push_back(all_forms[3]);

  // Same thing for a generic saved login.
  PasswordForm bar_example;
  bar_example.scheme = PasswordForm::SCHEME_HTML;
  bar_example.signon_realm = "http://bar.example.com";
  std::vector<PasswordForm*> bar_example_expected;
  bar_example_expected.push_back(all_forms[4]);

  MockPasswordStoreConsumer consumer;
  testing::InSequence s;
  EXPECT_CALL(consumer, OnGetPasswordStoreResultsConstRef(
                            ContainsSamePasswordForms(www_google_expected)))
      .RetiresOnSaturation();
  EXPECT_CALL(consumer,
              OnGetPasswordStoreResultsConstRef(ContainsSamePasswordForms(
                  accounts_google_expected))).RetiresOnSaturation();
  EXPECT_CALL(consumer, OnGetPasswordStoreResultsConstRef(
                            ContainsSamePasswordForms(bar_example_expected)))
      .RetiresOnSaturation();

  store->GetLogins(www_google, PasswordStore::ALLOW_PROMPT, &consumer);
  store->GetLogins(accounts_google, PasswordStore::ALLOW_PROMPT, &consumer);
  store->GetLogins(bar_example, PasswordStore::ALLOW_PROMPT, &consumer);

  base::MessageLoop::current()->RunUntilIdle();

  store->Shutdown();
  base::MessageLoop::current()->RunUntilIdle();
}

TEST_F(PasswordStoreTest, StartSyncFlare) {
  scoped_refptr<PasswordStoreDefault> store(new PasswordStoreDefault(
      base::MessageLoopProxy::current(), base::MessageLoopProxy::current(),
      make_scoped_ptr(new LoginDatabase(test_login_db_file_path()))));
  StartSyncFlareMock mock;
  store->Init(
      base::Bind(&StartSyncFlareMock::StartSyncFlare, base::Unretained(&mock)));
  {
    PasswordForm form;
    form.origin = GURL("http://accounts.google.com/LoginAuth");
    form.signon_realm = "http://accounts.google.com/";
    EXPECT_CALL(mock, StartSyncFlare(syncer::PASSWORDS));
    store->AddLogin(form);
    base::MessageLoop::current()->RunUntilIdle();
  }
  store->Shutdown();
  base::MessageLoop::current()->RunUntilIdle();
}

}  // namespace password_manager
