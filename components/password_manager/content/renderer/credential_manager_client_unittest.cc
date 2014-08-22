// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/content/renderer/credential_manager_client.h"
#include "components/password_manager/content/renderer/test_credential_manager_client.h"
#include "content/public/test/mock_render_thread.h"
#include "content/test/blink_test_environment.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebCredential.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerClient.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerError.h"

namespace password_manager {

namespace {

class CredentialManagerClientTest : public testing::Test {
 public:
  CredentialManagerClientTest()
      : callback_errored_(false),
        callback_succeeded_(false) {
    blink::WebString string = blink::WebString::fromUTF8("");
    GURL url("https://example.com/image");
    credential_.reset(new blink::WebCredential(string, string, url));
  }
  virtual ~CredentialManagerClientTest() {}

  static void SetUpTestCase() { content::SetUpBlinkTestEnvironment(); }

  static void TearDownTestCase() { content::TearDownBlinkTestEnvironment(); }

  IPC::TestSink& sink() { return render_thread_.sink(); }

  blink::WebCredential* credential() { return credential_.get(); }

  // The browser's response to any of the messages the client sends must contain
  // a request ID so that the client knows which request is being serviced. This
  // method grabs the ID from an outgoing |message_id| message, and sets the
  // |request_id| param to its value. If no request ID can be found, the method
  // returns false, and the |request_id| is set to -1.
  //
  // Clears any pending messages upon return.
  bool ExtractRequestId(uint32 message_id, int& request_id) {
    request_id = -1;
    const IPC::Message* message = sink().GetFirstMessageMatching(message_id);
    if (!message)
      return false;

    switch (message_id) {
      case CredentialManagerHostMsg_NotifyFailedSignIn::ID: {
        Tuple2<int, CredentialInfo> param;
        CredentialManagerHostMsg_NotifyFailedSignIn::Read(message, &param);
        request_id = param.a;
        break;
      }

      case CredentialManagerHostMsg_NotifySignedIn::ID: {
        Tuple2<int, CredentialInfo> param;
        CredentialManagerHostMsg_NotifySignedIn::Read(message, &param);
        request_id = param.a;
        break;
      }

      case CredentialManagerHostMsg_NotifySignedOut::ID: {
        Tuple1<int> param;
        CredentialManagerHostMsg_NotifySignedOut::Read(message, &param);
        request_id = param.a;
        break;
      }

      case CredentialManagerHostMsg_RequestCredential::ID: {
        Tuple3<int, bool, std::vector<GURL> > param;
        CredentialManagerHostMsg_RequestCredential::Read(message, &param);
        request_id = param.a;
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
  content::MockRenderThread render_thread_;
  TestCredentialManagerClient client_;

  // True if a message's callback's 'onSuccess'/'onError' methods were called,
  // false otherwise. We put these on the test object rather than on the
  // Test*Callbacks objects because ownership of those objects passes into the
  // client, which destroys the callbacks after calling them to resolve the
  // pending Blink-side Promise.
  bool callback_errored_;
  bool callback_succeeded_;

  scoped_ptr<blink::WebCredential> credential_;
};

class TestNotificationCallbacks
    : public blink::WebCredentialManagerClient::NotificationCallbacks {
 public:
  explicit TestNotificationCallbacks(CredentialManagerClientTest* test)
      : test_(test) {
  }

  virtual ~TestNotificationCallbacks() {}

  virtual void onSuccess() OVERRIDE { test_->set_callback_succeeded(true); }

  virtual void onError(blink::WebCredentialManagerError* reason) OVERRIDE {
    test_->set_callback_errored(true);
  }

 private:
  CredentialManagerClientTest* test_;
};

class TestRequestCallbacks
    : public blink::WebCredentialManagerClient::RequestCallbacks {
 public:
  explicit TestRequestCallbacks(CredentialManagerClientTest* test)
      : test_(test) {
  }

  virtual ~TestRequestCallbacks() {}

  virtual void onSuccess(blink::WebCredential*) OVERRIDE {
    test_->set_callback_succeeded(true);
  }

  virtual void onError(blink::WebCredentialManagerError* reason) OVERRIDE {
    test_->set_callback_errored(true);
  }

 private:
  CredentialManagerClientTest* test_;
};

}  // namespace

TEST_F(CredentialManagerClientTest, SendNotifyFailedSignIn) {
  int request_id;
  EXPECT_FALSE(ExtractRequestId(CredentialManagerHostMsg_NotifyFailedSignIn::ID,
                                request_id));

  scoped_ptr<TestNotificationCallbacks> callbacks(
      new TestNotificationCallbacks(this));
  client_.dispatchFailedSignIn(*credential(), callbacks.release());

  EXPECT_TRUE(ExtractRequestId(CredentialManagerHostMsg_NotifyFailedSignIn::ID,
                               request_id));

  client_.OnAcknowledgeFailedSignIn(request_id);
  EXPECT_TRUE(callback_succeeded());
  EXPECT_FALSE(callback_errored());
}

TEST_F(CredentialManagerClientTest, SendNotifySignedIn) {
  int request_id;
  EXPECT_FALSE(ExtractRequestId(CredentialManagerHostMsg_NotifySignedIn::ID,
                                request_id));

  scoped_ptr<TestNotificationCallbacks> callbacks(
      new TestNotificationCallbacks(this));
  client_.dispatchSignedIn(*credential(), callbacks.release());

  EXPECT_TRUE(ExtractRequestId(CredentialManagerHostMsg_NotifySignedIn::ID,
                               request_id));

  client_.OnAcknowledgeSignedIn(request_id);
  EXPECT_TRUE(callback_succeeded());
  EXPECT_FALSE(callback_errored());
}

TEST_F(CredentialManagerClientTest, SendNotifySignedOut) {
  int request_id;
  EXPECT_FALSE(ExtractRequestId(CredentialManagerHostMsg_NotifySignedOut::ID,
                                request_id));

  scoped_ptr<TestNotificationCallbacks> callbacks(
      new TestNotificationCallbacks(this));
  client_.dispatchSignedOut(callbacks.release());

  EXPECT_TRUE(ExtractRequestId(CredentialManagerHostMsg_NotifySignedOut::ID,
                               request_id));

  client_.OnAcknowledgeSignedOut(request_id);
  EXPECT_TRUE(callback_succeeded());
  EXPECT_FALSE(callback_errored());
}

TEST_F(CredentialManagerClientTest, SendRequestCredential) {
  int request_id;
  EXPECT_FALSE(ExtractRequestId(CredentialManagerHostMsg_RequestCredential::ID,
                                request_id));

  scoped_ptr<TestRequestCallbacks> callbacks(new TestRequestCallbacks(this));
  std::vector<GURL> federations;
  client_.dispatchRequest(false, federations, callbacks.release());

  EXPECT_TRUE(ExtractRequestId(CredentialManagerHostMsg_RequestCredential::ID,
                               request_id));

  CredentialInfo info;
  client_.OnSendCredential(request_id, info);
  EXPECT_TRUE(callback_succeeded());
  EXPECT_FALSE(callback_errored());
}

}  // namespace password_manager
