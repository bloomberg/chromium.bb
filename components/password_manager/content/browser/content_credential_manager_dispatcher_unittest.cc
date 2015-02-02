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
#include "components/password_manager/content/browser/credential_manager_password_form_manager.h"
#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;
using content::WebContents;

namespace {

// Chosen by fair dice roll. Guaranteed to be random.
const int kRequestId = 4;

class TestPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  TestPasswordManagerClient(password_manager::PasswordStore* store)
      : did_prompt_user_to_save_(false),
        did_prompt_user_to_choose_(false),
        is_off_the_record_(false),
        store_(store) {
    prefs_.registry()->RegisterBooleanPref(
        password_manager::prefs::kPasswordManagerAutoSignin, true);
  }
  ~TestPasswordManagerClient() override {}

  password_manager::PasswordStore* GetPasswordStore() override {
    return store_;
  }

  PrefService* GetPrefs() override { return &prefs_; }

  bool PromptUserToSavePassword(
      scoped_ptr<password_manager::PasswordFormManager> manager) override {
    did_prompt_user_to_save_ = true;
    manager_.reset(manager.release());
    return true;
  }

  bool PromptUserToChooseCredentials(
      const std::vector<autofill::PasswordForm*>& local_forms,
      const std::vector<autofill::PasswordForm*>& federated_forms,
      base::Callback<void(const password_manager::CredentialInfo&)> callback)
      override {
    // TODO(melandory): Use ScopedVector instead of std::vector in arguments.
    // ContentCredentialManagerDispatcher::OnGetPasswordStoreResults contains a
    // memory leak because of this.
    EXPECT_FALSE(local_forms.empty() && federated_forms.empty());
    did_prompt_user_to_choose_ = true;
    ScopedVector<autofill::PasswordForm> local_entries;
    local_entries.assign(local_forms.begin(), local_forms.end());
    ScopedVector<autofill::PasswordForm> federated_entries;
    federated_entries.assign(federated_forms.begin(), federated_forms.end());
    password_manager::CredentialInfo info(
        local_forms.empty() ? *federated_forms[0] : *local_entries[0],
        local_forms.empty()
            ? password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED
            : password_manager::CredentialType::CREDENTIAL_TYPE_LOCAL);
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, info));
    return true;
  }

  bool IsOffTheRecord() override { return is_off_the_record_; }

  bool did_prompt_user_to_save() const { return did_prompt_user_to_save_; }
  bool did_prompt_user_to_choose() const { return did_prompt_user_to_choose_; }

  password_manager::PasswordFormManager* pending_manager() const {
    return manager_.get();
  }

  void set_off_the_record(bool off_the_record) {
    is_off_the_record_ = off_the_record;
  }

  void set_zero_click_enabled(bool zero_click_enabled) {
    prefs_.SetBoolean(password_manager::prefs::kPasswordManagerAutoSignin,
                      zero_click_enabled);
  }

 private:
  TestingPrefServiceSimple prefs_;
  bool did_prompt_user_to_save_;
  bool did_prompt_user_to_choose_;
  bool is_off_the_record_;
  password_manager::PasswordStore* store_;
  scoped_ptr<password_manager::PasswordFormManager> manager_;
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
    client_.reset(new TestPasswordManagerClient(store_.get()));
    dispatcher_.reset(new TestCredentialManagerDispatcher(
        web_contents(), client_.get(), &stub_driver_));

    NavigateAndCommit(GURL("https://example.com/test.html"));

    form_.username_value = base::ASCIIToUTF16("Username");
    form_.display_name = base::ASCIIToUTF16("Display Name");
    form_.password_value = base::ASCIIToUTF16("Password");
    form_.origin = web_contents()->GetLastCommittedURL().GetOrigin();
    form_.signon_realm = form_.origin.spec();
    form_.scheme = autofill::PasswordForm::SCHEME_HTML;

    form2_.username_value = base::ASCIIToUTF16("Username 2");
    form2_.display_name = base::ASCIIToUTF16("Display Name 2");
    form2_.password_value = base::ASCIIToUTF16("Password 2");
    form2_.origin = web_contents()->GetLastCommittedURL().GetOrigin();
    form2_.signon_realm = form_.origin.spec();
    form2_.scheme = autofill::PasswordForm::SCHEME_HTML;

    cross_origin_form_.username_value = base::ASCIIToUTF16("Username");
    cross_origin_form_.display_name = base::ASCIIToUTF16("Display Name");
    cross_origin_form_.password_value = base::ASCIIToUTF16("Password");
    cross_origin_form_.origin = GURL("https://example.net/");
    cross_origin_form_.signon_realm = cross_origin_form_.origin.spec();
    cross_origin_form_.scheme = autofill::PasswordForm::SCHEME_HTML;

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
  autofill::PasswordForm form2_;
  autofill::PasswordForm cross_origin_form_;
  scoped_refptr<TestPasswordStore> store_;
  scoped_ptr<TestPasswordManagerClient> client_;
  StubPasswordManagerDriver stub_driver_;
  scoped_ptr<CredentialManagerDispatcher> dispatcher_;
};

TEST_F(CredentialManagerDispatcherTest, CredentialManagerOnNotifyFailedSignIn) {
  CredentialInfo info;
  info.type = CredentialType::CREDENTIAL_TYPE_LOCAL;
  dispatcher()->OnNotifyFailedSignIn(kRequestId, info);

  const uint32 kMsgID = CredentialManagerMsg_AcknowledgeFailedSignIn::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();
}

TEST_F(CredentialManagerDispatcherTest, CredentialManagerOnNotifySignedIn) {
  CredentialInfo info(form_,
                      password_manager::CredentialType::CREDENTIAL_TYPE_LOCAL);
  dispatcher()->OnNotifySignedIn(kRequestId, info);

  const uint32 kMsgID = CredentialManagerMsg_AcknowledgeSignedIn::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();

  // Allow the PasswordFormManager to talk to the password store, determine
  // that the form is new, and set it as pending.
  RunAllPendingTasks();

  EXPECT_TRUE(client_->did_prompt_user_to_save());
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

TEST_F(CredentialManagerDispatcherTest, CredentialManagerIncognitoSignedIn) {
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_LOCAL);
  client_->set_off_the_record(true);
  dispatcher()->OnNotifySignedIn(kRequestId, info);

  const uint32 kMsgID = CredentialManagerMsg_AcknowledgeSignedIn::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();

  RunAllPendingTasks();

  EXPECT_FALSE(client_->did_prompt_user_to_save());
  EXPECT_FALSE(client_->pending_manager());
}

TEST_F(CredentialManagerDispatcherTest, CredentialManagerOnNotifySignedOut) {
  dispatcher()->OnNotifySignedOut(kRequestId);

  const uint32 kMsgID = CredentialManagerMsg_AcknowledgeSignedOut::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();
}

TEST_F(CredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWithEmptyPasswordStore) {
  std::vector<GURL> federations;
  dispatcher()->OnRequestCredential(kRequestId, false, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  CredentialManagerMsg_SendCredential::Param param;
  CredentialManagerMsg_SendCredential::Read(message, &param);
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_EMPTY, get<1>(param).type);
  process()->sink().ClearMessages();
  EXPECT_FALSE(client_->did_prompt_user_to_choose());
}

TEST_F(CredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWithCrossOriginPasswordStore) {
  store_->AddLogin(cross_origin_form_);

  std::vector<GURL> federations;
  dispatcher()->OnRequestCredential(kRequestId, false, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  CredentialManagerMsg_SendCredential::Param param;
  CredentialManagerMsg_SendCredential::Read(message, &param);
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_EMPTY, get<1>(param).type);
  process()->sink().ClearMessages();
  EXPECT_FALSE(client_->did_prompt_user_to_choose());
}

TEST_F(CredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWithFullPasswordStore) {
  client_->set_zero_click_enabled(false);
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  dispatcher()->OnRequestCredential(kRequestId, false, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  EXPECT_TRUE(client_->did_prompt_user_to_choose());
}

TEST_F(
    CredentialManagerDispatcherTest,
    CredentialManagerOnRequestCredentialWithZeroClickOnlyEmptyPasswordStore) {
  std::vector<GURL> federations;
  dispatcher()->OnRequestCredential(kRequestId, true, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  EXPECT_FALSE(client_->did_prompt_user_to_choose());
  CredentialManagerMsg_SendCredential::Param send_param;
  CredentialManagerMsg_SendCredential::Read(message, &send_param);
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_EMPTY, get<1>(send_param).type);
}

TEST_F(CredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWithZeroClickOnlyFullPasswordStore) {
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  dispatcher()->OnRequestCredential(kRequestId, true, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  EXPECT_FALSE(client_->did_prompt_user_to_choose());
  CredentialManagerMsg_SendCredential::Param send_param;
  CredentialManagerMsg_SendCredential::Read(message, &send_param);
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_LOCAL, get<1>(send_param).type);
}

TEST_F(CredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWithZeroClickOnlyTwoPasswordStore) {
  store_->AddLogin(form_);
  store_->AddLogin(form2_);

  std::vector<GURL> federations;
  dispatcher()->OnRequestCredential(kRequestId, true, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  EXPECT_FALSE(client_->did_prompt_user_to_choose());
  CredentialManagerMsg_SendCredential::Param send_param;
  CredentialManagerMsg_SendCredential::Read(message, &send_param);

  // With two items in the password store, we shouldn't get credentials back.
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_EMPTY, get<1>(send_param).type);
}

TEST_F(CredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWhileRequestPending) {
  client_->set_zero_click_enabled(false);
  store_->AddLogin(form_);

  std::vector<GURL> federations;
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
            get<1>(reject_param));
  EXPECT_FALSE(client_->did_prompt_user_to_choose());

  process()->sink().ClearMessages();

  // Execute the PasswordStore asynchronousness.
  RunAllPendingTasks();

  // Check that the first request resolves.
  kMsgID = CredentialManagerMsg_SendCredential::ID;
  message = process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  CredentialManagerMsg_SendCredential::Param send_param;
  CredentialManagerMsg_SendCredential::Read(message, &send_param);
  EXPECT_NE(CredentialType::CREDENTIAL_TYPE_EMPTY, get<1>(send_param).type);
  process()->sink().ClearMessages();
  EXPECT_TRUE(client_->did_prompt_user_to_choose());
}

TEST_F(CredentialManagerDispatcherTest, IncognitoRequestCredential) {
  client_->set_off_the_record(true);
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  dispatcher()->OnRequestCredential(kRequestId, true, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  ASSERT_TRUE(message);
  CredentialManagerMsg_SendCredential::Param param;
  CredentialManagerMsg_SendCredential::Read(message, &param);
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_EMPTY, get<1>(param).type);
  EXPECT_FALSE(client_->did_prompt_user_to_choose());
}

}  // namespace password_manager
