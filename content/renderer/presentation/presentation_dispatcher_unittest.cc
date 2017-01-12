// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/renderer/presentation/presentation_dispatcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationAvailabilityObserver.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationError.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationSessionInfo.h"
#include "third_party/WebKit/public/web/WebArrayBuffer.h"

using ::testing::_;
using ::testing::Invoke;
using blink::WebArrayBuffer;
using blink::WebPresentationAvailabilityCallbacks;
using blink::WebPresentationAvailabilityObserver;
using blink::WebPresentationConnectionCallback;
using blink::WebPresentationError;
using blink::WebPresentationSessionInfo;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;
using blink::mojom::PresentationConnection;
using blink::mojom::PresentationError;
using blink::mojom::PresentationErrorPtr;
using blink::mojom::PresentationErrorType;
using blink::mojom::PresentationService;
using blink::mojom::PresentationServiceClientPtr;
using blink::mojom::PresentationSessionInfo;
using blink::mojom::PresentationSessionInfoPtr;
using blink::mojom::ConnectionMessage;
using blink::mojom::ConnectionMessagePtr;

// TODO(crbug.com/576808): Add test cases for the following:
// - State changes
// - Messages received
// - Discarding queued messages when the frame navigates
// - Screen availability not supported
// - Default presentation starting

namespace content {

class MockPresentationAvailabilityObserver
    : public WebPresentationAvailabilityObserver {
 public:
  explicit MockPresentationAvailabilityObserver(WebURL url) : url_(url) {}
  ~MockPresentationAvailabilityObserver() override {}

  MOCK_METHOD1(availabilityChanged, void(bool is_available));
  const WebURL url() const override { return url_; }

 private:
  const WebURL url_;
};

class MockPresentationService : public PresentationService {
 public:
  void SetClient(PresentationServiceClientPtr client) override {}
  MOCK_METHOD1(SetDefaultPresentationUrls,
               void(const std::vector<GURL>& presentation_urls));
  MOCK_METHOD1(ListenForScreenAvailability, void(const GURL& availability_url));
  MOCK_METHOD1(StopListeningForScreenAvailability,
               void(const GURL& availability_url));
  MOCK_METHOD2(StartSession,
               void(const std::vector<GURL>& presentation_urls,
                    const StartSessionCallback& callback));
  MOCK_METHOD3(JoinSession,
               void(const std::vector<GURL>& presentation_urls,
                    const base::Optional<std::string>& presentation_id,
                    const JoinSessionCallback& callback));

  // *Internal method is to work around lack of support for move-only types in
  // GMock.
  void SendConnectionMessage(
      PresentationSessionInfoPtr session_info,
      ConnectionMessagePtr message_request,
      const SendConnectionMessageCallback& callback) override {
    SendConnectionMessageInternal(session_info.get(), message_request.get(),
                                  callback);
  }
  MOCK_METHOD3(SendConnectionMessageInternal,
               void(PresentationSessionInfo* session_info,
                    ConnectionMessage* message_request,
                    const SendConnectionMessageCallback& callback));

  MOCK_METHOD2(CloseConnection,
               void(const GURL& presentation_url,
                    const std::string& presentation_id));
  MOCK_METHOD2(Terminate,
               void(const GURL& presentation_url,
                    const std::string& presentation_id));

  // *Internal method is to work around lack of support for move-only types in
  // GMock.
  void ListenForConnectionMessages(
      PresentationSessionInfoPtr session_info) override {
    ListenForConnectionMessagesInternal(session_info.get());
  }
  MOCK_METHOD1(ListenForConnectionMessagesInternal,
               void(PresentationSessionInfo* session_info));

  void SetPresentationConnection(
      blink::mojom::PresentationSessionInfoPtr session,
      blink::mojom::PresentationConnectionPtr controller_conn_ptr,
      blink::mojom::PresentationConnectionRequest receiver_conn_request)
      override {
    SetPresentationConnection(session.get(), controller_conn_ptr.get());
  }
  MOCK_METHOD2(SetPresentationConnection,
               void(PresentationSessionInfo* session_info,
                    PresentationConnection* connection));
};

class TestWebPresentationConnectionCallback
    : public WebPresentationConnectionCallback {
 public:
  TestWebPresentationConnectionCallback(WebURL url, WebString id)
      : url_(url), id_(id), callback_called_(false) {}
  ~TestWebPresentationConnectionCallback() override {
    EXPECT_TRUE(callback_called_);
  }

  void onSuccess(const WebPresentationSessionInfo& info) override {
    callback_called_ = true;
    EXPECT_EQ(info.url, url_);
    EXPECT_EQ(info.id, id_);
  }

 private:
  const WebURL url_;
  const WebString id_;
  bool callback_called_;
};

class TestWebPresentationConnectionErrorCallback
    : public WebPresentationConnectionCallback {
 public:
  TestWebPresentationConnectionErrorCallback(
      WebPresentationError::ErrorType error_type,
      WebString message)
      : error_type_(error_type), message_(message), callback_called_(false) {}
  ~TestWebPresentationConnectionErrorCallback() override {
    EXPECT_TRUE(callback_called_);
  }

  void onError(const WebPresentationError& error) override {
    callback_called_ = true;
    EXPECT_EQ(error.errorType, error_type_);
    EXPECT_EQ(error.message, message_);
  }

 private:
  const WebPresentationError::ErrorType error_type_;
  const WebString message_;
  bool callback_called_;
};

class TestPresentationDispatcher : public PresentationDispatcher {
 public:
  explicit TestPresentationDispatcher(
      MockPresentationService* presentation_service)
      : PresentationDispatcher(nullptr),
        mock_presentation_service_(presentation_service) {}
  ~TestPresentationDispatcher() override {}

 private:
  void ConnectToPresentationServiceIfNeeded() override {
    if (!mock_binding_) {
      mock_binding_ = base::MakeUnique<mojo::Binding<PresentationService>>(
          mock_presentation_service_,
          mojo::MakeRequest(&presentation_service_));
    }
  }

  MockPresentationService* mock_presentation_service_;
  std::unique_ptr<mojo::Binding<PresentationService>> mock_binding_;
};

class PresentationDispatcherTest : public ::testing::Test {
 public:
  PresentationDispatcherTest()
      : gurl1_(GURL("https://www.example.com/1.html")),
        gurl2_(GURL("https://www.example.com/2.html")),
        gurls_({gurl1_, gurl2_}),
        url1_(WebURL(gurl1_)),
        url2_(WebURL(gurl2_)),
        urls_(WebVector<WebURL>(gurls_)),
        presentation_id_(WebString::fromUTF8("test-id")),
        array_buffer_(WebArrayBuffer::create(4, 1)),
        observer_(url1_),
        dispatcher_(&presentation_service_) {}
  ~PresentationDispatcherTest() override {}

  void SetUp() override {
    // Set some test data.
    *array_buffer_data() = 42;
  }

  uint8_t* array_buffer_data() {
    return static_cast<uint8_t*>(array_buffer_.data());
  }

 protected:
  const GURL gurl1_;
  const GURL gurl2_;
  const std::vector<GURL> gurls_;
  const WebURL url1_;
  const WebURL url2_;
  const WebVector<WebURL> urls_;
  const WebString presentation_id_;
  const WebArrayBuffer array_buffer_;
  MockPresentationAvailabilityObserver observer_;
  MockPresentationService presentation_service_;
  TestPresentationDispatcher dispatcher_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(PresentationDispatcherTest, TestStartSession) {
  base::RunLoop run_loop;

  EXPECT_CALL(presentation_service_, StartSession(gurls_, _))
      .WillOnce(Invoke([this](
          const std::vector<GURL>& presentation_urls,
          const PresentationService::StartSessionCallback& callback) {
        PresentationSessionInfoPtr session_info(PresentationSessionInfo::New());
        session_info->url = gurl1_;
        session_info->id = presentation_id_.utf8();
        callback.Run(std::move(session_info), PresentationErrorPtr());
      }));
  dispatcher_.startSession(
      urls_, base::MakeUnique<TestWebPresentationConnectionCallback>(
                 url1_, presentation_id_));
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestStartSessionError) {
  WebString error_message = WebString::fromUTF8("Test error message");
  base::RunLoop run_loop;

  EXPECT_CALL(presentation_service_, StartSession(gurls_, _))
      .WillOnce(Invoke([this, &error_message](
          const std::vector<GURL>& presentation_urls,
          const PresentationService::StartSessionCallback& callback) {
        PresentationErrorPtr error(PresentationError::New());
        error->error_type = PresentationErrorType::NO_AVAILABLE_SCREENS;
        error->message = error_message.utf8();
        callback.Run(PresentationSessionInfoPtr(), std::move(error));
      }));
  dispatcher_.startSession(
      urls_,
      base::MakeUnique<TestWebPresentationConnectionErrorCallback>(
          WebPresentationError::ErrorTypeNoAvailableScreens, error_message));
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestJoinSessionError) {
  WebString error_message = WebString::fromUTF8("Test error message");
  base::RunLoop run_loop;

  EXPECT_CALL(presentation_service_, JoinSession(gurls_, _, _))
      .WillOnce(Invoke([this, &error_message](
          const std::vector<GURL>& presentation_urls,
          const base::Optional<std::string>& presentation_id,
          const PresentationService::JoinSessionCallback& callback) {
        EXPECT_TRUE(presentation_id.has_value());
        EXPECT_EQ(presentation_id_.utf8(), presentation_id.value());
        PresentationErrorPtr error(PresentationError::New());
        error->error_type = PresentationErrorType::NO_AVAILABLE_SCREENS;
        error->message = error_message.utf8();
        callback.Run(PresentationSessionInfoPtr(), std::move(error));
      }));
  dispatcher_.joinSession(
      urls_, presentation_id_,
      base::MakeUnique<TestWebPresentationConnectionErrorCallback>(
          WebPresentationError::ErrorTypeNoAvailableScreens, error_message));
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestJoinSession) {
  base::RunLoop run_loop;

  EXPECT_CALL(presentation_service_, JoinSession(gurls_, _, _))
      .WillOnce(Invoke([this](
          const std::vector<GURL>& presentation_urls,
          const base::Optional<std::string>& presentation_id,
          const PresentationService::JoinSessionCallback& callback) {
        EXPECT_TRUE(presentation_id.has_value());
        EXPECT_EQ(presentation_id_.utf8(), presentation_id.value());
        PresentationSessionInfoPtr session_info(PresentationSessionInfo::New());
        session_info->url = gurl1_;
        session_info->id = presentation_id_.utf8();
        callback.Run(std::move(session_info), PresentationErrorPtr());
      }));
  dispatcher_.joinSession(
      urls_, presentation_id_,
      base::MakeUnique<TestWebPresentationConnectionCallback>(
          url1_, presentation_id_));
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestSendString) {
  WebString message = WebString::fromUTF8("test message");
  base::RunLoop run_loop;
  EXPECT_CALL(presentation_service_, SendConnectionMessageInternal(_, _, _))
      .WillOnce(Invoke([this, &message](
          PresentationSessionInfo* session_info,
          ConnectionMessage* message_request,
          const PresentationService::SendConnectionMessageCallback& callback) {
        EXPECT_EQ(gurl1_, session_info->url);
        EXPECT_EQ(presentation_id_.utf8(), session_info->id);
        EXPECT_TRUE(message_request->message.has_value());
        EXPECT_EQ(message.utf8(), message_request->message.value());
        callback.Run(true);
      }));
  dispatcher_.sendString(url1_, presentation_id_, message);
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestSendArrayBuffer) {
  base::RunLoop run_loop;
  EXPECT_CALL(presentation_service_, SendConnectionMessageInternal(_, _, _))
      .WillOnce(Invoke([this](
          PresentationSessionInfo* session_info,
          ConnectionMessage* message_request,
          const PresentationService::SendConnectionMessageCallback& callback) {
        EXPECT_EQ(gurl1_, session_info->url);
        EXPECT_EQ(presentation_id_.utf8(), session_info->id);
        std::vector<uint8_t> data(
            array_buffer_data(),
            array_buffer_data() + array_buffer_.byteLength());
        EXPECT_TRUE(message_request->data.has_value());
        EXPECT_EQ(data, message_request->data.value());
        callback.Run(true);
      }));
  dispatcher_.sendArrayBuffer(url1_, presentation_id_, array_buffer_data(),
                              array_buffer_.byteLength());
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestSendBlobData) {
  base::RunLoop run_loop;
  EXPECT_CALL(presentation_service_, SendConnectionMessageInternal(_, _, _))
      .WillOnce(Invoke([this](
          PresentationSessionInfo* session_info,
          ConnectionMessage* message_request,
          const PresentationService::SendConnectionMessageCallback& callback) {
        EXPECT_EQ(gurl1_, session_info->url);
        EXPECT_EQ(presentation_id_.utf8(), session_info->id);
        std::vector<uint8_t> data(
            array_buffer_data(),
            array_buffer_data() + array_buffer_.byteLength());
        EXPECT_TRUE(message_request->data.has_value());
        EXPECT_EQ(data, message_request->data.value());
        callback.Run(true);
      }));
  dispatcher_.sendBlobData(url1_, presentation_id_, array_buffer_data(),
                           array_buffer_.byteLength());
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestCloseSession) {
  base::RunLoop run_loop;
  EXPECT_CALL(presentation_service_,
              CloseConnection(gurl1_, presentation_id_.utf8()));
  dispatcher_.closeSession(url1_, presentation_id_);
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestTerminateSession) {
  base::RunLoop run_loop;
  EXPECT_CALL(presentation_service_,
              Terminate(gurl1_, presentation_id_.utf8()));
  dispatcher_.terminateSession(url1_, presentation_id_);
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestListenForScreenAvailability) {
  base::RunLoop run_loop1;
  EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl1_));
  dispatcher_.getAvailability(
      url1_, base::MakeUnique<WebPresentationAvailabilityCallbacks>());
  dispatcher_.OnScreenAvailabilityUpdated(url1_, true);
  run_loop1.RunUntilIdle();

  base::RunLoop run_loop2;
  EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl1_));
  dispatcher_.startListening(&observer_);
  run_loop2.RunUntilIdle();

  base::RunLoop run_loop3;
  EXPECT_CALL(observer_, availabilityChanged(false));
  dispatcher_.OnScreenAvailabilityUpdated(url1_, false);
  EXPECT_CALL(observer_, availabilityChanged(true));
  dispatcher_.OnScreenAvailabilityUpdated(url1_, true);
  EXPECT_CALL(presentation_service_,
              StopListeningForScreenAvailability(gurl1_));
  dispatcher_.stopListening(&observer_);
  run_loop3.RunUntilIdle();

  // After stopListening(), |observer_| should no longer be notified.
  base::RunLoop run_loop4;
  EXPECT_CALL(observer_, availabilityChanged(false)).Times(0);
  dispatcher_.OnScreenAvailabilityUpdated(url1_, false);
  run_loop4.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestSetDefaultPresentationUrls) {
  base::RunLoop run_loop;
  EXPECT_CALL(presentation_service_, SetDefaultPresentationUrls(gurls_));
  dispatcher_.setDefaultPresentationUrls(urls_);
  run_loop.RunUntilIdle();
}

}  // namespace content
