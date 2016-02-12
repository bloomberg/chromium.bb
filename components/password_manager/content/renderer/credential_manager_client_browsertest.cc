// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/content/renderer/credential_manager_client.h"
#include "content/public/test/render_view_test.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebCredential.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerClient.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerError.h"
#include "third_party/WebKit/public/platform/WebPassOwnPtr.h"
#include "third_party/WebKit/public/platform/WebPasswordCredential.h"

namespace password_manager {

namespace {

class CredentialManagerClientTest : public content::RenderViewTest {
 public:
  CredentialManagerClientTest()
      : callback_errored_(false), callback_succeeded_(false) {}
  ~CredentialManagerClientTest() override {}

  void SetUp() override {
    content::RenderViewTest::SetUp();
    credential_.reset(new blink::WebPasswordCredential("", "", "", GURL()));
    client_.reset(new CredentialManagerClient(view_));
  }

  void TearDown() override {
    credential_.reset();
    client_.reset();
    content::RenderViewTest::TearDown();
  }

  IPC::TestSink& sink() { return render_thread_->sink(); }

  blink::WebCredential* credential() { return credential_.get(); }

  // The browser's response to any of the messages the client sends must contain
  // a request ID so that the client knows which request is being serviced. This
  // method grabs the ID from an outgoing |message_id| message, and sets the
  // |request_id| param to its value. If no request ID can be found, the method
  // returns false, and the |request_id| is set to -1.
  //
  // Clears any pending messages upon return.
  bool ExtractRequestId(uint32_t message_id, int& request_id) {
    request_id = -1;
    const IPC::Message* message = sink().GetFirstMessageMatching(message_id);
    if (!message)
      return false;

    switch (message_id) {
      case CredentialManagerHostMsg_Store::ID: {
        base::Tuple<int, CredentialInfo> param;
        CredentialManagerHostMsg_Store::Read(message, &param);
        request_id = base::get<0>(param);
        break;
      }

      case CredentialManagerHostMsg_RequireUserMediation::ID: {
        base::Tuple<int> param;
        CredentialManagerHostMsg_RequireUserMediation::Read(message, &param);
        request_id = base::get<0>(param);
        break;
      }

      case CredentialManagerHostMsg_RequestCredential::ID: {
        base::Tuple<int, bool, bool, std::vector<GURL>> param;
        CredentialManagerHostMsg_RequestCredential::Read(message, &param);
        request_id = base::get<0>(param);
        break;
      }

      default:
        break;
    }
    sink().ClearMessages();
    return request_id != -1;
  }

  bool callback_errored() const { return callback_errored_; }
  void set_callback_errored(bool state) { callback_errored_ = state; }
  bool callback_succeeded() const { return callback_succeeded_; }
  void set_callback_succeeded(bool state) { callback_succeeded_ = state; }

 protected:
  scoped_ptr<CredentialManagerClient> client_;

  // True if a message's callback's 'onSuccess'/'onError' methods were called,
  // false otherwise. We put these on the test object rather than on the
  // Test*Callbacks objects because ownership of those objects passes into the
  // client, which destroys the callbacks after calling them to resolve the
  // pending Blink-side Promise.
  bool callback_errored_;
  bool callback_succeeded_;

  scoped_ptr<blink::WebPasswordCredential> credential_;
};

class TestNotificationCallbacks
    : public blink::WebCredentialManagerClient::NotificationCallbacks {
 public:
  explicit TestNotificationCallbacks(CredentialManagerClientTest* test)
      : test_(test) {}

  ~TestNotificationCallbacks() override {}

  void onSuccess() override { test_->set_callback_succeeded(true); }

  void onError(blink::WebCredentialManagerError reason) override {
    test_->set_callback_errored(true);
  }

 private:
  CredentialManagerClientTest* test_;
};

class TestRequestCallbacks
    : public blink::WebCredentialManagerClient::RequestCallbacks {
 public:
  explicit TestRequestCallbacks(CredentialManagerClientTest* test)
      : test_(test) {}

  ~TestRequestCallbacks() override {}

  void onSuccess(blink::WebPassOwnPtr<blink::WebCredential>) override {
    test_->set_callback_succeeded(true);
  }

  void onError(blink::WebCredentialManagerError reason) override {
    test_->set_callback_errored(true);
  }

 private:
  CredentialManagerClientTest* test_;
};

}  // namespace

TEST_F(CredentialManagerClientTest, SendStore) {
  int request_id;
  EXPECT_FALSE(
      ExtractRequestId(CredentialManagerHostMsg_Store::ID, request_id));

  scoped_ptr<TestNotificationCallbacks> callbacks(
      new TestNotificationCallbacks(this));
  client_->dispatchStore(*credential(), callbacks.release());

  EXPECT_TRUE(ExtractRequestId(CredentialManagerHostMsg_Store::ID, request_id));

  client_->OnAcknowledgeStore(request_id);
  EXPECT_TRUE(callback_succeeded());
  EXPECT_FALSE(callback_errored());
}

TEST_F(CredentialManagerClientTest, SendRequestUserMediation) {
  int request_id;
  EXPECT_FALSE(ExtractRequestId(
      CredentialManagerHostMsg_RequireUserMediation::ID, request_id));

  scoped_ptr<TestNotificationCallbacks> callbacks(
      new TestNotificationCallbacks(this));
  client_->dispatchRequireUserMediation(callbacks.release());

  EXPECT_TRUE(ExtractRequestId(
      CredentialManagerHostMsg_RequireUserMediation::ID, request_id));

  client_->OnAcknowledgeRequireUserMediation(request_id);
  EXPECT_TRUE(callback_succeeded());
  EXPECT_FALSE(callback_errored());
}

TEST_F(CredentialManagerClientTest, SendRequestCredential) {
  int request_id;
  EXPECT_FALSE(ExtractRequestId(CredentialManagerHostMsg_RequestCredential::ID,
                                request_id));

  scoped_ptr<TestRequestCallbacks> callbacks(new TestRequestCallbacks(this));
  std::vector<GURL> federations;
  client_->dispatchGet(false, true, federations, callbacks.release());

  EXPECT_TRUE(ExtractRequestId(CredentialManagerHostMsg_RequestCredential::ID,
                               request_id));

  CredentialInfo info;
  info.type = CredentialType::CREDENTIAL_TYPE_PASSWORD;
  client_->OnSendCredential(request_id, info);
  EXPECT_TRUE(callback_succeeded());
  EXPECT_FALSE(callback_errored());
}

TEST_F(CredentialManagerClientTest, SendRequestCredentialEmpty) {
  int request_id;
  EXPECT_FALSE(ExtractRequestId(CredentialManagerHostMsg_RequestCredential::ID,
                                request_id));

  scoped_ptr<TestRequestCallbacks> callbacks(new TestRequestCallbacks(this));
  std::vector<GURL> federations;
  client_->dispatchGet(false, true, federations, callbacks.release());

  EXPECT_TRUE(ExtractRequestId(CredentialManagerHostMsg_RequestCredential::ID,
                               request_id));

  CredentialInfo info;  // Send an empty credential in response.
  client_->OnSendCredential(request_id, info);
  EXPECT_TRUE(callback_succeeded());
  EXPECT_FALSE(callback_errored());
}

}  // namespace password_manager
