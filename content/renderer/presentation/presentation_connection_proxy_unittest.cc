// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/renderer/presentation/presentation_connection_proxy.h"
#include "content/renderer/presentation/test_presentation_connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnection.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationInfo.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationReceiver.h"

using ::testing::_;

namespace content {

class MockWebPresentationReceiver : public blink::WebPresentationReceiver {
 public:
  MOCK_METHOD1(
      OnReceiverConnectionAvailable,
      blink::WebPresentationConnection*(const blink::WebPresentationInfo&));
  MOCK_METHOD1(DidChangeConnectionState,
               void(blink::WebPresentationConnectionState));
  MOCK_METHOD0(TerminateConnection, void());
  MOCK_METHOD1(RemoveConnection, void(blink::WebPresentationConnection*));
};

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
    controller_connection_->BindProxy(
        base::WrapUnique(controller_connection_proxy_));
    receiver_connection_proxy_ = new ReceiverConnectionProxy(
        receiver_connection_.get(), &mock_receiver_);
    receiver_connection_->BindProxy(
        base::WrapUnique(receiver_connection_proxy_));

    EXPECT_CALL(
        *controller_connection_,
        DidChangeState(blink::WebPresentationConnectionState::kConnected));
    EXPECT_CALL(
        *receiver_connection_,
        DidChangeState(blink::WebPresentationConnectionState::kConnected));

    receiver_connection_proxy_->Bind(
        controller_connection_proxy_->MakeRemoteRequest());
    receiver_connection_proxy_->BindControllerConnection(
        controller_connection_proxy_->Bind());
  }

  void ExpectBinaryMessageReceived(const std::vector<uint8_t>& expected_data) {
    EXPECT_CALL(*receiver_connection_, DidReceiveBinaryMessage(_, _))
        .WillOnce(::testing::Invoke(
            [&expected_data](const uint8_t* data, size_t length) {
              std::vector<uint8_t> message_data(data, data + length);
              EXPECT_EQ(expected_data, message_data);
            }));
  }

  void TearDown() override {
    controller_connection_.reset();
    receiver_connection_.reset();
  }

 protected:
  std::unique_ptr<TestPresentationConnection> controller_connection_;
  std::unique_ptr<TestPresentationConnection> receiver_connection_;
  ControllerConnectionProxy* controller_connection_proxy_;
  ReceiverConnectionProxy* receiver_connection_proxy_;
  MockWebPresentationReceiver mock_receiver_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(PresentationConnectionProxyTest, TestSendString) {
  blink::WebString message = blink::WebString::FromUTF8("test message");
  base::RunLoop run_loop;
  EXPECT_CALL(*receiver_connection_, DidReceiveTextMessage(message));
  controller_connection_proxy_->SendTextMessage(message);
  run_loop.RunUntilIdle();
}

TEST_F(PresentationConnectionProxyTest, TestSendLongString) {
  blink::WebString message =
      blink::WebString::FromUTF8(std::string(65537u, 'w'));
  base::RunLoop run_loop;
  EXPECT_CALL(*receiver_connection_, DidReceiveTextMessage(message));
  controller_connection_proxy_->SendTextMessage(message);
  run_loop.RunUntilIdle();
}

TEST_F(PresentationConnectionProxyTest, TestSendArrayBuffer) {
  std::vector<uint8_t> expected_data;
  expected_data.push_back(42);
  expected_data.push_back(36);

  base::RunLoop run_loop;
  ExpectBinaryMessageReceived(expected_data);
  controller_connection_proxy_->SendBinaryMessage(expected_data.data(),
                                                  expected_data.size());
  run_loop.RunUntilIdle();
}

TEST_F(PresentationConnectionProxyTest, TestSendLongArrayBuffer) {
  std::vector<uint8_t> expected_data(65537u, 42);
  base::RunLoop run_loop;
  ExpectBinaryMessageReceived(expected_data);
  controller_connection_proxy_->SendBinaryMessage(expected_data.data(),
                                                  expected_data.size());
  run_loop.RunUntilIdle();
}

TEST_F(PresentationConnectionProxyTest, TestControllerConnectionCallsClose) {
  base::RunLoop run_loop;
  EXPECT_CALL(*controller_connection_, DidClose());
  EXPECT_CALL(*receiver_connection_, DidClose());
  EXPECT_CALL(mock_receiver_, RemoveConnection(receiver_connection_.get()));
  controller_connection_proxy_->Close();
  run_loop.RunUntilIdle();
}

TEST_F(PresentationConnectionProxyTest, TestReceiverConnectionCallsClose) {
  base::RunLoop run_loop;
  EXPECT_CALL(*controller_connection_, DidClose());
  EXPECT_CALL(*receiver_connection_, DidClose());
  EXPECT_CALL(mock_receiver_, RemoveConnection(receiver_connection_.get()));
  receiver_connection_proxy_->Close();
  run_loop.RunUntilIdle();
}

TEST_F(PresentationConnectionProxyTest, TestReceiverNotifyTargetConnection) {
  base::RunLoop run_loop;
  EXPECT_CALL(
      *controller_connection_,
      DidChangeState(blink::WebPresentationConnectionState::kTerminated));
  receiver_connection_proxy_->NotifyTargetConnection(
      blink::WebPresentationConnectionState::kTerminated);
  run_loop.RunUntilIdle();
}

}  // namespace content
