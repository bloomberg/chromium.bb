// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/browser_presentation_connection_proxy.h"

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace media_router {

constexpr char kMediaRouteId[] = "MockRouteId";

class MockPresentationConnectionProxy
    : public NON_EXPORTED_BASE(blink::mojom::PresentationConnection) {
 public:
  void OnMessage(blink::mojom::ConnectionMessagePtr message,
                 const OnMessageCallback& on_message_callback) override {
    OnMessageRaw(message.get(), on_message_callback);
  }

  MOCK_METHOD2(OnMessageRaw,
               void(const blink::mojom::ConnectionMessage*,
                    const OnMessageCallback&));
  MOCK_METHOD1(DidChangeState,
               void(content::PresentationConnectionState state));
  MOCK_METHOD0(OnClose, void());
};

class BrowserPresentationConnectionProxyTest : public ::testing::Test {
 public:
  BrowserPresentationConnectionProxyTest() = default;

  void SetUp() override {
    mock_controller_connection_proxy_ =
        base::MakeUnique<MockPresentationConnectionProxy>();
    blink::mojom::PresentationConnectionPtr controller_connection_ptr;
    mojo::Binding<blink::mojom::PresentationConnection> binding(
        mock_controller_connection_proxy_.get(),
        mojo::MakeRequest(&controller_connection_ptr));
    EXPECT_CALL(
        *mock_controller_connection_proxy_,
        DidChangeState(content::PRESENTATION_CONNECTION_STATE_CONNECTED));

    blink::mojom::PresentationConnectionPtr receiver_connection_ptr;

    base::RunLoop run_loop;
    browser_connection_proxy_ =
        base::MakeUnique<BrowserPresentationConnectionProxy>(
            &mock_router_, "MockRouteId",
            mojo::MakeRequest(&receiver_connection_ptr),
            std::move(controller_connection_ptr));
    run_loop.RunUntilIdle();
  }

  void TearDown() override {
    browser_connection_proxy_.reset();
    mock_controller_connection_proxy_.reset();
  }

  BrowserPresentationConnectionProxy* browser_connection_proxy() {
    return browser_connection_proxy_.get();
  }

  MockMediaRouter* mock_router() { return &mock_router_; }

 private:
  std::unique_ptr<MockPresentationConnectionProxy>
      mock_controller_connection_proxy_;
  std::unique_ptr<BrowserPresentationConnectionProxy> browser_connection_proxy_;
  MockMediaRouter mock_router_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(BrowserPresentationConnectionProxyTest, TestOnMessageTextMessage) {
  std::string message = "test message";
  blink::mojom::ConnectionMessagePtr session_message =
      blink::mojom::ConnectionMessage::New();
  session_message->type = blink::mojom::PresentationMessageType::TEXT;
  session_message->message = message;

  base::MockCallback<base::Callback<void(bool)>> mock_on_message_callback;
  EXPECT_CALL(*mock_router(), SendRouteMessage(kMediaRouteId, message, _));

  browser_connection_proxy()->OnMessage(std::move(session_message),
                                        mock_on_message_callback.Get());
}

TEST_F(BrowserPresentationConnectionProxyTest, TestOnMessageBinaryMessage) {
  std::vector<uint8_t> expected_data;
  expected_data.push_back(42);
  expected_data.push_back(36);

  blink::mojom::ConnectionMessagePtr session_message =
      blink::mojom::ConnectionMessage::New();
  session_message->type = blink::mojom::PresentationMessageType::BINARY;
  session_message->data = expected_data;

  base::MockCallback<base::Callback<void(bool)>> mock_on_message_callback;
  EXPECT_CALL(*mock_router(), SendRouteBinaryMessageInternal(_, _, _))
      .WillOnce(::testing::Invoke([&expected_data](
          const MediaRoute::Id& route_id, std::vector<uint8_t>* data,
          const BrowserPresentationConnectionProxy::OnMessageCallback&
              callback) { EXPECT_EQ(expected_data, *data); }));

  browser_connection_proxy()->OnMessage(std::move(session_message),
                                        mock_on_message_callback.Get());
}

}  // namespace media_router
