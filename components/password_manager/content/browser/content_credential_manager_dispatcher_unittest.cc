// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_credential_manager_dispatcher.h"

#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;
using content::WebContents;

namespace {

// Chosen by fair dice roll. Guaranteed to be random.
const int kRequestId = 4;

}  // namespace

namespace password_manager {

class ContentCredentialManagerDispatcherTest
    : public content::RenderViewHostTestHarness {
 public:
  ContentCredentialManagerDispatcherTest() {}

  virtual void SetUp() OVERRIDE {
    content::RenderViewHostTestHarness::SetUp();
    dispatcher_.reset(
        new ContentCredentialManagerDispatcher(web_contents(), nullptr));
  }

  ContentCredentialManagerDispatcher* dispatcher() { return dispatcher_.get(); }

 private:
  scoped_ptr<ContentCredentialManagerDispatcher> dispatcher_;
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
       CredentialManagerOnRequestCredential) {
  std::vector<GURL> federations;
  dispatcher()->OnRequestCredential(kRequestId, false, federations);

  const uint32 kMsgID = CredentialManagerMsg_SendCredential::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  EXPECT_TRUE(message);
  process()->sink().ClearMessages();
}

}  // namespace password_manager
