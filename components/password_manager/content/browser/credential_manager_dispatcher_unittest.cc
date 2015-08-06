// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/credential_manager_dispatcher.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/core/browser/credential_manager_password_form_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;
using content::WebContents;

using testing::_;

namespace {

// Chosen by fair dice roll. Guaranteed to be random.
const int kRequestId = 4;

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_CONST_METHOD0(IsSavingEnabledForCurrentPage, bool());
  MOCK_CONST_METHOD0(IsOffTheRecord, bool());
  MOCK_METHOD1(NotifyUserAutoSigninPtr,
               bool(const std::vector<autofill::PasswordForm*>& local_forms));
  MOCK_METHOD2(PromptUserToSavePasswordPtr,
               void(password_manager::PasswordFormManager*,
                    password_manager::CredentialSourceType type));
  MOCK_METHOD4(PromptUserToChooseCredentialsPtr,
               bool(const std::vector<autofill::PasswordForm*>& local_forms,
                    const std::vector<autofill::PasswordForm*>& federated_forms,
                    const GURL& origin,
                    base::Callback<void(
                        const password_manager::CredentialInfo&)> callback));

  MockPasswordManagerClient(password_manager::PasswordStore* store)
      : store_(store) {
    prefs_.registry()->RegisterBooleanPref(
        password_manager::prefs::kPasswordManagerAutoSignin, true);
  }
  ~MockPasswordManagerClient() override {}

  bool PromptUserToSaveOrUpdatePassword(
      scoped_ptr<password_manager::PasswordFormManager> manager,
      password_manager::CredentialSourceType type,
      bool update_password) override {
    manager_.swap(manager);
    PromptUserToSavePasswordPtr(manager_.get(), type);
    return true;
  }

  password_manager::PasswordStore* GetPasswordStore() const override {
    return store_;
  }

  PrefService* GetPrefs() override { return &prefs_; }

  bool PromptUserToChooseCredentials(
      ScopedVector<autofill::PasswordForm> local_forms,
      ScopedVector<autofill::PasswordForm> federated_forms,
      const GURL& origin,
      base::Callback<void(const password_manager::CredentialInfo&)> callback) {
    EXPECT_FALSE(local_forms.empty() && federated_forms.empty());
    password_manager::CredentialInfo info(
        local_forms.empty() ? *federated_forms[0] : *local_forms[0],
        local_forms.empty()
            ? password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED
            : password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, info));
    PromptUserToChooseCredentialsPtr(local_forms.get(), federated_forms.get(),
                                     origin, callback);
    return true;
  }

  void NotifyUserAutoSignin(
      ScopedVector<autofill::PasswordForm> local_forms) override {
    EXPECT_FALSE(local_forms.empty());
    NotifyUserAutoSigninPtr(local_forms.get());
  }

  password_manager::PasswordFormManager* pending_manager() const {
    return manager_.get();
  }

  void set_zero_click_enabled(bool zero_click_enabled) {
    prefs_.SetBoolean(password_manager::prefs::kPasswordManagerAutoSignin,
                      zero_click_enabled);
  }

 private:
  TestingPrefServiceSimple prefs_;
  password_manager::PasswordStore* store_;
  scoped_ptr<password_manager::PasswordFormManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerClient);
};

class TestCredentialManagerDispatcher
    : public password_manager::CredentialManagerDispatcher {
 public:
  TestCredentialManagerDispatcher(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client,
      password_manager::PasswordManagerDriver* driver);

 private:
  base::WeakPtr<password_manager::PasswordManagerDriver> GetDriver() override;

  base::WeakPtr<password_manager::PasswordManagerDriver> driver_;
};

TestCredentialManagerDispatcher::TestCredentialManagerDispatcher(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    password_manager::PasswordManagerDriver* driver)
    : CredentialManagerDispatcher(web_contents, client),
      driver_(driver->AsWeakPtr()) {
}

base::WeakPtr<password_manager::PasswordManagerDriver>
TestCredentialManagerDispatcher::GetDriver() {
  return driver_;
}

void RunAllPendingTasks() {
  base::RunLoop run_loop;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  run_loop.Run();
}

}  // namespace

namespace password_manager {

class CredentialManagerDispatcherTest
    : public content::RenderViewHostTestHarness {
 public:
  CredentialManagerDispatcherTest() {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    store_ = new TestPasswordStore;
    client_.reset(new MockPasswordManagerClient(store_.get()));
    dispatcher_.reset(new TestCredentialManagerDispatcher(
        web_contents(), client_.get(), &stub_driver_));
    ON_CALL(*client_, IsSavingEnabledForCurrentPage())
        .WillByDefault(testing::Return(true));
    ON_CALL(*client_, IsOffTheRecord()).WillByDefault(testing::Return(false));

    NavigateAndCommit(GURL("https://example.com/test.html"));

    form_.username_value = base::ASCIIToUTF16("Username");
    form_.display_name = base::ASCIIToUTF16("Display Name");
    form_.password_value = base::ASCIIToUTF16("Password");
    form_.origin = web_contents()->GetLastCommittedURL().GetOrigin();
    form_.signon_realm = form_.origin.spec();
    form_.scheme = autofill::PasswordForm::SCHEME_HTML;
    form_.skip_zero_click = false;

    origin_path_form_.username_value = base::ASCIIToUTF16("Username 2");
    origin_path_form_.display_name = base::ASCIIToUTF16("Display Name 2");
    origin_path_form_.password_value = base::ASCIIToUTF16("Password 2");
    origin_path_form_.origin = GURL("https://example.com/path");
    origin_path_form_.signon_realm = origin_path_form_.origin.spec();
    origin_path_form_.scheme = autofill::PasswordForm::SCHEME_HTML;
    origin_path_form_.skip_zero_click = false;

    cross_origin_form_.username_value = base::ASCIIToUTF16("Username");
    cross_origin_form_.display_name = base::ASCIIToUTF16("Display Name");
    cross_origin_form_.password_value = base::ASCIIToUTF16("Password");
    cross_origin_form_.origin = GURL("https://example.net/");
    cross_origin_form_.signon_realm = cross_origin_form_.origin.spec();
    cross_origin_form_.scheme = autofill::PasswordForm::SCHEME_HTML;
    cross_origin_form_.skip_zero_click = false;

    store_->Clear();
    EXPECT_TRUE(store_->IsEmpty());
  }

  void TearDown() override {
    store_->Shutdown();
    content::RenderViewHostTestHarness::TearDown();
  }

  CredentialManagerDispatcher* dispatcher() { return dispatcher_.get(); }

 protected:
  autofill::PasswordForm form_;
  autofill::PasswordForm origin_path_form_;
  autofill::PasswordForm cross_origin_form_;
  scoped_refptr<TestPasswordStore> store_;
  scoped_ptr<MockPasswordManagerClient> client_;
  StubPasswordManagerDriver stub_driver_;
  scoped_ptr<CredentialManagerDispatcher> dispatcher_;
};

TEST_F(CredentialManagerDispatcherTest, CredentialManagerOnStore) {
  CredentialInfo info(
      form_, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_CALL(
      *client_,
      PromptUserToSavePasswordPtr(
          _, password_manager::CredentialSourceType::CREDENTIAL_SOURCE_API))
      .Times(testing::Exactly(1));

  dispatcher()->OnStore(kRequestId, info);

  const uint32 kMsgID = CredentialManagerMsg_AcknowledgeStore::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();

  // Allow the PasswordFormManager to talk to the password store, determine
  // that the form is new, and set it as pending.
  RunAllPendingTasks();

  EXPECT_TRUE(client_->pending_manager()->HasCompletedMatching());

  autofill::PasswordForm new_form =
      client_->pending_manager()->pending_credentials();
  EXPECT_EQ(form_.username_value, new_form.username_value);
  EXPECT_EQ(form_.display_name, new_form.display_name);
  EXPECT_EQ(form_.password_value, new_form.password_value);
  EXPECT_EQ(form_.origin, new_form.origin);
  EXPECT_EQ(form_.signon_realm, new_form.signon_realm);
  EXPECT_EQ(autofill::PasswordForm::SCHEME_HTML, new_form.scheme);
}

TEST_F(CredentialManagerDispatcherTest,
       CredentialManagerSignInWithSavingDisabledForCurrentPage) {
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_CALL(*client_, IsSavingEnabledForCurrentPage())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(
      *client_,
      PromptUserToSavePasswordPtr(
          _, password_manager::CredentialSourceType::CREDENTIAL_SOURCE_API))
      .Times(testing::Exactly(0));

  dispatcher()->OnStore(kRequestId, info);

  const uint32 kMsgID = CredentialManagerMsg_AcknowledgeStore::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();

  RunAllPendingTasks();

  EXPECT_FALSE(client_->pending_manager());
}

TEST_F(CredentialManagerDispatcherTest,
       CredentialManagerOnRequireUserMediation) {
  store_->AddLogin(form_);
  store_->AddLogin(cross_origin_form_);
  RunAllPendingTasks();

  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(2U, passwords.size());
  EXPECT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[cross_origin_form_.signon_realm].size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);

  dispatcher()->OnRequireUserMediation(kRequestId);
  RunAllPendingTasks();

  const uint32 kMsgID =
      CredentialManagerMsg_AcknowledgeRequireUserMediation::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();

  passwords = store_->stored_passwords();
  EXPECT_EQ(2U, passwords.size());
  EXPECT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[cross_origin_form_.signon_realm].size());
  EXPECT_TRUE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWithEmptyPasswordStore) {
  std::vector<GURL> federations;
  EXPECT_CALL(
      *client_,
      PromptUserToSavePasswordPtr(
          _, password_manager::CredentialSourceType::CREDENTIAL_SOURCE_API))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  dispatcher()->OnRequestCredential(kRequestId, false, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  CredentialManagerMsg_SendCredential::Param param;
  CredentialManagerMsg_SendCredential::Read(message, &param);
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_EMPTY, base::get<1>(param).type);
  process()->sink().ClearMessages();
}

TEST_F(CredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWithCrossOriginPasswordStore) {
  store_->AddLogin(cross_origin_form_);

  std::vector<GURL> federations;
  EXPECT_CALL(
      *client_,
      PromptUserToSavePasswordPtr(
          _, password_manager::CredentialSourceType::CREDENTIAL_SOURCE_API))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  dispatcher()->OnRequestCredential(kRequestId, false, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  CredentialManagerMsg_SendCredential::Param param;
  CredentialManagerMsg_SendCredential::Read(message, &param);
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_EMPTY, base::get<1>(param).type);
  process()->sink().ClearMessages();
}

TEST_F(CredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWithFullPasswordStore) {
  client_->set_zero_click_enabled(false);
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  dispatcher()->OnRequestCredential(kRequestId, false, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
}

TEST_F(
    CredentialManagerDispatcherTest,
    CredentialManagerOnRequestCredentialWithZeroClickOnlyEmptyPasswordStore) {
  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  dispatcher()->OnRequestCredential(kRequestId, true, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  CredentialManagerMsg_SendCredential::Param send_param;
  CredentialManagerMsg_SendCredential::Read(message, &send_param);
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_EMPTY,
            base::get<1>(send_param).type);
}

TEST_F(CredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWithZeroClickOnlyFullPasswordStore) {
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(1));

  dispatcher()->OnRequestCredential(kRequestId, true, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  CredentialManagerMsg_SendCredential::Param send_param;
  CredentialManagerMsg_SendCredential::Read(message, &send_param);
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_PASSWORD,
            base::get<1>(send_param).type);
}

TEST_F(CredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWithZeroClickOnlyTwoPasswordStore) {
  store_->AddLogin(form_);
  store_->AddLogin(origin_path_form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  dispatcher()->OnRequestCredential(kRequestId, true, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  CredentialManagerMsg_SendCredential::Param send_param;
  CredentialManagerMsg_SendCredential::Read(message, &send_param);

  // With two items in the password store, we shouldn't get credentials back.
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_EMPTY,
            base::get<1>(send_param).type);
}

TEST_F(CredentialManagerDispatcherTest,
       OnRequestCredentialWithZeroClickOnlyOnePasswordStore) {
  form_.skip_zero_click = true;
  store_->AddLogin(form_);
  store_->AddLogin(origin_path_form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(1));

  dispatcher()->OnRequestCredential(kRequestId, true, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  CredentialManagerMsg_SendCredential::Param send_param;
  CredentialManagerMsg_SendCredential::Read(message, &send_param);

  // We should get |origin_path_form_| back, as |form_| is marked as skipping
  // zero-click.
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_PASSWORD,
            base::get<1>(send_param).type);
  EXPECT_EQ(origin_path_form_.username_value, base::get<1>(send_param).id);
  EXPECT_EQ(origin_path_form_.display_name, base::get<1>(send_param).name);
  EXPECT_EQ(origin_path_form_.password_value,
            base::get<1>(send_param).password);
}

TEST_F(CredentialManagerDispatcherTest,
       OnRequestCredentialWithZeroClickOnlyCrossOriginPasswordStore) {
  store_->AddLogin(cross_origin_form_);

  form_.skip_zero_click = true;
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  dispatcher()->OnRequestCredential(kRequestId, true, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  CredentialManagerMsg_SendCredential::Param send_param;
  CredentialManagerMsg_SendCredential::Read(message, &send_param);

  // We only have cross-origin zero-click credentials; they should not be
  // returned.
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_EMPTY,
            base::get<1>(send_param).type);
}

TEST_F(CredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWhileRequestPending) {
  client_->set_zero_click_enabled(false);
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  dispatcher()->OnRequestCredential(kRequestId, false, federations);
  dispatcher()->OnRequestCredential(kRequestId, false, federations);

  // Check that the second request triggered a rejection.
  uint32 kMsgID = CredentialManagerMsg_RejectCredentialRequest::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);

  CredentialManagerMsg_RejectCredentialRequest::Param reject_param;
  CredentialManagerMsg_RejectCredentialRequest::Read(message, &reject_param);
  EXPECT_EQ(blink::WebCredentialManagerError::ErrorTypePendingRequest,
            base::get<1>(reject_param));
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  process()->sink().ClearMessages();

  // Execute the PasswordStore asynchronousness.
  RunAllPendingTasks();

  // Check that the first request resolves.
  kMsgID = CredentialManagerMsg_SendCredential::ID;
  message = process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  CredentialManagerMsg_SendCredential::Param send_param;
  CredentialManagerMsg_SendCredential::Read(message, &send_param);
  EXPECT_NE(CredentialType::CREDENTIAL_TYPE_EMPTY,
            base::get<1>(send_param).type);
  process()->sink().ClearMessages();
}

TEST_F(CredentialManagerDispatcherTest, ResetSkipZeroClickAfterPrompt) {
  // Turn on the global zero-click flag, and add two credentials in separate
  // origins, both set to skip zero-click.
  client_->set_zero_click_enabled(true);
  form_.skip_zero_click = true;
  store_->AddLogin(form_);
  cross_origin_form_.skip_zero_click = true;
  store_->AddLogin(cross_origin_form_);

  // Execute the PasswordStore asynchronousness to ensure everything is
  // written before proceeding.
  RunAllPendingTasks();

  // Sanity check.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(2U, passwords.size());
  EXPECT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[cross_origin_form_.signon_realm].size());
  EXPECT_TRUE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_TRUE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);

  // Trigger a request which should return the credential found in |form_|, and
  // wait for it to process.
  std::vector<GURL> federations;
  // Check that the form in the database has been updated. `OnRequestCredential`
  // generates a call to prompt the user to choose a credential.
  // MockPasswordManagerClient mocks a user choice, and when users choose a
  // credential (and have the global zero-click flag enabled), we make sure that
  // they'll be logged in again next time.
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  dispatcher()->OnRequestCredential(kRequestId, false, federations);
  RunAllPendingTasks();

  passwords = store_->stored_passwords();
  EXPECT_EQ(2U, passwords.size());
  EXPECT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[cross_origin_form_.signon_realm].size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_TRUE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerDispatcherTest, IncognitoZeroClickRequestCredential) {
  EXPECT_CALL(*client_, IsOffTheRecord()).WillRepeatedly(testing::Return(true));
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr(_)).Times(testing::Exactly(0));

  dispatcher()->OnRequestCredential(kRequestId, true, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  ASSERT_TRUE(message);
  CredentialManagerMsg_SendCredential::Param param;
  CredentialManagerMsg_SendCredential::Read(message, &param);
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_EMPTY, base::get<1>(param).type);
}

}  // namespace password_manager
