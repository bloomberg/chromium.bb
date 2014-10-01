// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_credential_manager_dispatcher.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/test_password_store.h"
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
      : store_(store) {}
  virtual ~TestPasswordManagerClient() {}

  virtual password_manager::PasswordStore* GetPasswordStore() OVERRIDE {
    return store_;
  }

 private:
  password_manager::PasswordStore* store_;
};

void RunAllPendingTasks() {
  base::RunLoop run_loop;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  run_loop.Run();
}

}  // namespace

namespace password_manager {

class ContentCredentialManagerDispatcherTest
    : public content::RenderViewHostTestHarness {
 public:
  ContentCredentialManagerDispatcherTest() {}

  virtual void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    store_ = new TestPasswordStore;
    client_.reset(new TestPasswordManagerClient(store_.get()));
    dispatcher_.reset(
        new ContentCredentialManagerDispatcher(web_contents(), client_.get()));

    NavigateAndCommit(GURL("https://example.com/test.html"));

    form_.username_value = base::ASCIIToUTF16("Username");
    form_.display_name = base::ASCIIToUTF16("Display Name");
    form_.password_value = base::ASCIIToUTF16("Password");
    form_.origin = web_contents()->GetLastCommittedURL().GetOrigin();
    form_.signon_realm = form_.origin.spec();
    form_.scheme = autofill::PasswordForm::SCHEME_HTML;
  }

  virtual void TearDown() OVERRIDE {
    store_->Shutdown();
    content::RenderViewHostTestHarness::TearDown();
  }

  ContentCredentialManagerDispatcher* dispatcher() { return dispatcher_.get(); }

 protected:
  autofill::PasswordForm form_;
  scoped_refptr<TestPasswordStore> store_;
  scoped_ptr<ContentCredentialManagerDispatcher> dispatcher_;
  scoped_ptr<TestPasswordManagerClient> client_;
};

TEST_F(ContentCredentialManagerDispatcherTest,
       CredentialManagerOnNotifyFailedSignIn) {
  CredentialInfo info(base::ASCIIToUTF16("id"),
                      base::ASCIIToUTF16("name"),
                      GURL("https://example.com/image.png"));
  dispatcher()->OnNotifyFailedSignIn(kRequestId, info);

  const uint32 kMsgID = CredentialManagerMsg_AcknowledgeFailedSignIn::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();
}

TEST_F(ContentCredentialManagerDispatcherTest,
       CredentialManagerOnNotifySignedIn) {
  CredentialInfo info(base::ASCIIToUTF16("id"),
                      base::ASCIIToUTF16("name"),
                      GURL("https://example.com/image.png"));
  dispatcher()->OnNotifySignedIn(kRequestId, info);

  const uint32 kMsgID = CredentialManagerMsg_AcknowledgeSignedIn::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();
}

TEST_F(ContentCredentialManagerDispatcherTest,
       CredentialManagerOnNotifySignedOut) {
  dispatcher()->OnNotifySignedOut(kRequestId);

  const uint32 kMsgID = CredentialManagerMsg_AcknowledgeSignedOut::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();
}

TEST_F(ContentCredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWithEmptyPasswordStore) {
  std::vector<GURL> federations;
  dispatcher()->OnRequestCredential(kRequestId, false, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_RejectCredentialRequest::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();
}

TEST_F(ContentCredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWithFullPasswordStore) {
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  dispatcher()->OnRequestCredential(kRequestId, false, federations);

  RunAllPendingTasks();

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();
}

TEST_F(ContentCredentialManagerDispatcherTest,
       CredentialManagerOnRequestCredentialWhileRequestPending) {
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  dispatcher()->OnRequestCredential(kRequestId, false, federations);
  dispatcher()->OnRequestCredential(kRequestId, false, federations);

  // Check that the second request triggered a rejection.
  uint32 kMsgID = CredentialManagerMsg_RejectCredentialRequest::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();

  // Execute the PasswordStore asynchronousness.
  RunAllPendingTasks();

  // Check that the first request resolves.
  kMsgID = CredentialManagerMsg_SendCredential::ID;
  message = process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();
}

}  // namespace password_manager
