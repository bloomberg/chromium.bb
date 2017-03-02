// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/renderer/presentation/presentation_connection_proxy.h"
#include "content/renderer/presentation/test_presentation_connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnection.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"

using ::testing::_;

namespace content {

class PresentationConnectionProxyTest : public ::testing::Test {
 public:
  PresentationConnectionProxyTest() = default;
  ~PresentationConnectionProxyTest() override = default;

  void SetUp() override {
    // Set up test connections and test connection proxies.
    controller_connection_ = base::MakeUnique<TestPresentationConnection>();
    receiver_connection_ = base::MakeUnique<TestPresentationConnection>();

    controller_connection_proxy_ =
        new ControllerConnectionProxy(controller_connection_.get());
    controller_connection_->bindProxy(
        base::WrapUnique(controller_connection_proxy_));
    receiver_connection_proxy_ =
        new ReceiverConnectionProxy(receiver_connection_.get());
    receiver_connection_->bindProxy(
        base::WrapUnique(receiver_connection_proxy_));

    EXPECT_CALL(
        *controller_connection_,
        didChangeState(blink::WebPresentationConnectionState::Connected));
    EXPECT_CALL(
        *receiver_connection_,
        didChangeState(blink::WebPresentationConnectionState::Connected));

    receiver_connection_proxy_->Bind(
        controller_connection_proxy_->MakeRemoteRequest());
    receiver_connection_proxy_->BindControllerConnection(
        controller_connection_proxy_->Bind());
  }

  void TearDown() override {
    controller_connection_.reset();
    receiver_connection_.reset();
  }

  void ExpectSendConnectionMessageCallback(bool success) {
    EXPECT_TRUE(success);
  }

 protected:
  std::unique_ptr<TestPresentationConnection> controller_connection_;
  std::unique_ptr<TestPresentationConnection> receiver_connection_;
  ControllerConnectionProxy* controller_connection_proxy_;
  ReceiverConnectionProxy* receiver_connection_proxy_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(PresentationConnectionProxyTest, TestSendString) {
  blink::WebString message = blink::WebString::fromUTF8("test message");
  blink::mojom::ConnectionMessagePtr session_message =
      blink::mojom::ConnectionMessage::New();
  session_message->type = blink::mojom::PresentationMessageType::TEXT;
  session_message->message = message.utf8();

  base::RunLoop run_loop;
  EXPECT_CALL(*receiver_connection_, didReceiveTextMessage(message));
  controller_connection_proxy_->SendConnectionMessage(
      std::move(session_message),
      base::Bind(
          &PresentationConnectionProxyTest::ExpectSendConnectionMessageCallback,
          base::Unretained(this)));
  run_loop.RunUntilIdle();
}

TEST_F(PresentationConnectionProxyTest, TestSendArrayBuffer) {
  std::vector<uint8_t> expected_data;
  expected_data.push_back(42);
  expected_data.push_back(36);

  blink::mojom::ConnectionMessagePtr session_message =
      blink::mojom::ConnectionMessage::New();
  session_message->type = blink::mojom::PresentationMessageType::BINARY;
  session_message->data = expected_data;

  base::RunLoop run_loop;
  EXPECT_CALL(*receiver_connection_, didReceiveBinaryMessage(_, _))
      .WillOnce(::testing::Invoke(
          [&expected_data](const uint8_t* data, size_t length) {
            std::vector<uint8_t> message_data(data, data + length);
            EXPECT_EQ(expected_data, message_data);
          }));

  controller_connection_proxy_->SendConnectionMessage(
      std::move(session_message),
      base::Bind(
          &PresentationConnectionProxyTest::ExpectSendConnectionMessageCallback,
          base::Unretained(this)));
  run_loop.RunUntilIdle();
}

TEST_F(PresentationConnectionProxyTest, TestControllerConnectionCallsClose) {
  base::RunLoop run_loop;
  EXPECT_CALL(*controller_connection_,
              didChangeState(blink::WebPresentationConnectionState::Closed));
  EXPECT_CALL(*receiver_connection_,
              didChangeState(blink::WebPresentationConnectionState::Closed));
  controller_connection_proxy_->close();
  run_loop.RunUntilIdle();
}

TEST_F(PresentationConnectionProxyTest, TestReceiverConnectionCallsClose) {
  base::RunLoop run_loop;
  EXPECT_CALL(*controller_connection_,
              didChangeState(blink::WebPresentationConnectionState::Closed));
  EXPECT_CALL(*receiver_connection_,
              didChangeState(blink::WebPresentationConnectionState::Closed));
  receiver_connection_proxy_->close();
  run_loop.RunUntilIdle();
}

}  // namespace content
