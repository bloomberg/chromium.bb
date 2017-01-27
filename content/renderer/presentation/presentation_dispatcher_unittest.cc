// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/renderer/presentation/presentation_connection_proxy.h"
#include "content/renderer/presentation/presentation_dispatcher.h"
#include "content/renderer/presentation/test_presentation_connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationAvailabilityObserver.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnection.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnectionCallbacks.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationError.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationSessionInfo.h"
#include "third_party/WebKit/public/web/WebArrayBuffer.h"

using ::testing::_;
using ::testing::Invoke;
using blink::WebArrayBuffer;
using blink::WebPresentationAvailabilityCallbacks;
using blink::WebPresentationAvailabilityObserver;
using blink::WebPresentationConnectionCallbacks;
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
  explicit MockPresentationAvailabilityObserver(const std::vector<GURL>& urls)
      : urls_(urls) {}
  ~MockPresentationAvailabilityObserver() override {}

  MOCK_METHOD1(availabilityChanged, void(bool is_available));
  const WebVector<WebURL>& urls() const override { return urls_; }

 private:
  const WebVector<WebURL> urls_;
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

class MockPresentationAvailabilityCallbacks
    : public blink::WebCallbacks<bool, const blink::WebPresentationError&> {
 public:
  MOCK_METHOD1(onSuccess, void(bool value));
  MOCK_METHOD1(onError, void(const blink::WebPresentationError&));
};

class TestWebPresentationConnectionCallback
    : public WebPresentationConnectionCallbacks {
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

  blink::WebPresentationConnection* getConnection() override {
    return &connection_;
  }

 private:
  const WebURL url_;
  const WebString id_;
  bool callback_called_;
  TestPresentationConnection connection_;
};

class TestWebPresentationConnectionErrorCallback
    : public WebPresentationConnectionCallbacks {
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

  blink::WebPresentationConnection* getConnection() override { return nullptr; }

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
  enum class URLState { Available, Unavailable, Unsupported, Unknown };

  PresentationDispatcherTest()
      : gurl1_(GURL("https://www.example.com/1.html")),
        gurl2_(GURL("https://www.example.com/2.html")),
        gurl3_(GURL("https://www.example.com/3.html")),
        gurl4_(GURL("https://www.example.com/4.html")),
        gurls_({gurl1_, gurl2_, gurl3_, gurl4_}),
        url1_(WebURL(gurl1_)),
        url2_(WebURL(gurl2_)),
        url3_(WebURL(gurl3_)),
        url4_(WebURL(gurl4_)),
        urls_(WebVector<WebURL>(gurls_)),
        presentation_id_(WebString::fromUTF8("test-id")),
        array_buffer_(WebArrayBuffer::create(4, 1)),
        observer_(gurls_),
        mock_observer1_({gurl1_, gurl2_, gurl3_}),
        mock_observer2_({gurl2_, gurl3_, gurl4_}),
        mock_observer3_({gurl2_, gurl3_}),
        mock_observers_({&mock_observer1_, &mock_observer2_, &mock_observer3_}),
        dispatcher_(&presentation_service_) {}

  ~PresentationDispatcherTest() override {}

  void SetUp() override {
    // Set some test data.
    *array_buffer_data() = 42;
  }

  uint8_t* array_buffer_data() {
    return static_cast<uint8_t*>(array_buffer_.data());
  }

  void ChangeURLState(const GURL& url, URLState state) {
    switch (state) {
      case URLState::Available:
        dispatcher_.OnScreenAvailabilityUpdated(url, true);
        break;
      case URLState::Unavailable:
        dispatcher_.OnScreenAvailabilityUpdated(url, false);
        break;
      case URLState::Unsupported:
        dispatcher_.OnScreenAvailabilityNotSupported(url);
        break;
      case URLState::Unknown:
        break;
    }
  }

  // Tests that PresenationService is called for getAvailability(urls), after
  // |urls| change state to |states|. This function takes ownership of
  // |mock_callback|.
  void TestGetAvailability(
      const std::vector<GURL>& urls,
      const std::vector<URLState>& states,
      MockPresentationAvailabilityCallbacks* mock_callback) {
    DCHECK_EQ(urls.size(), states.size());

    for (const auto& url : urls) {
      EXPECT_CALL(presentation_service_, ListenForScreenAvailability(url))
          .Times(1);
      EXPECT_CALL(presentation_service_,
                  StopListeningForScreenAvailability(url))
          .Times(1);
    }

    base::RunLoop run_loop;
    client()->getAvailability(urls, base::WrapUnique(mock_callback));
    for (size_t i = 0; i < urls.size(); i++)
      ChangeURLState(urls[i], states[i]);

    run_loop.RunUntilIdle();
  }

  blink::WebPresentationClient* client() { return &dispatcher_; }

 protected:
  const GURL gurl1_;
  const GURL gurl2_;
  const GURL gurl3_;
  const GURL gurl4_;
  const std::vector<GURL> gurls_;
  const WebURL url1_;
  const WebURL url2_;
  const WebURL url3_;
  const WebURL url4_;
  const WebVector<WebURL> urls_;
  const WebString presentation_id_;
  const WebArrayBuffer array_buffer_;
  MockPresentationAvailabilityObserver observer_;
  MockPresentationAvailabilityObserver mock_observer1_;
  MockPresentationAvailabilityObserver mock_observer2_;
  MockPresentationAvailabilityObserver mock_observer3_;
  std::vector<MockPresentationAvailabilityObserver*> mock_observers_;

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
  client()->startSession(
      urls_, base::MakeUnique<TestWebPresentationConnectionCallback>(
                 url1_, presentation_id_));
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestStartSessionError) {
  WebString error_message = WebString::fromUTF8("Test error message");
  base::RunLoop run_loop;

  EXPECT_CALL(presentation_service_, StartSession(gurls_, _))
      .WillOnce(Invoke([&error_message](
          const std::vector<GURL>& presentation_urls,
          const PresentationService::StartSessionCallback& callback) {
        PresentationErrorPtr error(PresentationError::New());
        error->error_type = PresentationErrorType::NO_AVAILABLE_SCREENS;
        error->message = error_message.utf8();
        callback.Run(PresentationSessionInfoPtr(), std::move(error));
      }));
  client()->startSession(
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
  dispatcher_.sendString(url1_, presentation_id_, message, nullptr);
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
                              array_buffer_.byteLength(), nullptr);
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
                           array_buffer_.byteLength(), nullptr);
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
  for (const auto& gurl : gurls_) {
    EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl));
    EXPECT_CALL(presentation_service_,
                StopListeningForScreenAvailability(gurl));
  }

  dispatcher_.getAvailability(
      urls_, base::MakeUnique<WebPresentationAvailabilityCallbacks>());
  dispatcher_.OnScreenAvailabilityUpdated(url1_, true);
  run_loop1.RunUntilIdle();

  base::RunLoop run_loop2;
  for (const auto& gurl : gurls_)
    EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl));

  client()->startListening(&observer_);
  run_loop2.RunUntilIdle();

  base::RunLoop run_loop3;
  EXPECT_CALL(observer_, availabilityChanged(false));
  dispatcher_.OnScreenAvailabilityUpdated(url1_, false);
  EXPECT_CALL(observer_, availabilityChanged(true));
  dispatcher_.OnScreenAvailabilityUpdated(url1_, true);
  for (const auto& gurl : gurls_) {
    EXPECT_CALL(presentation_service_,
                StopListeningForScreenAvailability(gurl));
  }
  client()->stopListening(&observer_);
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

TEST_F(PresentationDispatcherTest, GetAvailabilityOneUrlNoAvailabilityChange) {
  auto* mock_callback =
      new testing::StrictMock<MockPresentationAvailabilityCallbacks>();

  EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl1_))
      .Times(1);

  base::RunLoop run_loop;
  client()->getAvailability(std::vector<GURL>({gurl1_}),
                            base::WrapUnique(mock_callback));
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, GetAvailabilityOneUrlBecomesAvailable) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, onSuccess(true));

  TestGetAvailability({url1_}, {URLState::Available}, mock_callback);
}

TEST_F(PresentationDispatcherTest, GetAvailabilityOneUrlBecomesUnavailable) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, onSuccess(false));

  TestGetAvailability({url1_}, {URLState::Unavailable}, mock_callback);
}

TEST_F(PresentationDispatcherTest, GetAvailabilityOneUrlBecomesNotSupported) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, onError(_));

  TestGetAvailability({url1_}, {URLState::Unsupported}, mock_callback);
}

TEST_F(PresentationDispatcherTest,
       GetAvailabilityMultipleUrlsAllBecomesAvailable) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, onSuccess(true)).Times(1);

  TestGetAvailability({url1_, url2_},
                      {URLState::Available, URLState::Available},
                      mock_callback);
}

TEST_F(PresentationDispatcherTest,
       GetAvailabilityMultipleUrlsAllBecomesUnavailable) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, onSuccess(false)).Times(1);

  TestGetAvailability({url1_, url2_},
                      {URLState::Unavailable, URLState::Unavailable},
                      mock_callback);
}

TEST_F(PresentationDispatcherTest,
       GetAvailabilityMultipleUrlsAllBecomesUnsupported) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, onError(_)).Times(1);

  TestGetAvailability({url1_, url2_},
                      {URLState::Unsupported, URLState::Unsupported},
                      mock_callback);
}

TEST_F(PresentationDispatcherTest,
       GetAvailabilityReturnsDirectlyForAlreadyListeningUrls) {
  // First getAvailability() call.
  auto* mock_callback_1 = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback_1, onSuccess(false)).Times(1);

  std::vector<URLState> state_seq = {URLState::Unavailable, URLState::Available,
                                     URLState::Unavailable};
  TestGetAvailability({url1_, url2_, url3_}, state_seq, mock_callback_1);

  // Second getAvailability() call.
  for (const auto& url : mock_observer3_.urls()) {
    EXPECT_CALL(presentation_service_, ListenForScreenAvailability((GURL)url))
        .Times(1);
  }
  auto* mock_callback_2 = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback_2, onSuccess(true)).Times(1);

  base::RunLoop run_loop;
  client()->getAvailability(mock_observer3_.urls(),
                            base::WrapUnique(mock_callback_2));
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, StartListeningListenToEachURLOnce) {
  for (const auto& gurl : gurls_) {
    EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl))
        .Times(1);
  }

  base::RunLoop run_loop;
  for (auto* mock_observer : mock_observers_) {
    client()->getAvailability(
        mock_observer->urls(),
        base::MakeUnique<WebPresentationAvailabilityCallbacks>());
    client()->startListening(mock_observer);
  }
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, StopListeningListenToEachURLOnce) {
  for (const auto& gurl : gurls_) {
    EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl))
        .Times(1);
    EXPECT_CALL(presentation_service_, StopListeningForScreenAvailability(gurl))
        .Times(1);
  }

  EXPECT_CALL(mock_observer1_, availabilityChanged(false));
  EXPECT_CALL(mock_observer2_, availabilityChanged(false));
  EXPECT_CALL(mock_observer3_, availabilityChanged(false));

  // Set up |availability_set_| and |listening_status_|
  base::RunLoop run_loop;
  for (auto* mock_observer : mock_observers_) {
    client()->getAvailability(
        mock_observer->urls(),
        base::MakeUnique<WebPresentationAvailabilityCallbacks>());

    client()->startListening(mock_observer);
  }

  // Clean up callbacks.
  ChangeURLState(gurl2_, URLState::Unavailable);

  for (auto* mock_observer : mock_observers_)
    client()->stopListening(mock_observer);

  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest,
       StopListeningDoesNotStopIfURLListenedByOthers) {
  for (const auto& gurl : gurls_) {
    EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl))
        .Times(1);
  }
  EXPECT_CALL(presentation_service_, StopListeningForScreenAvailability(gurl1_))
      .Times(1);
  EXPECT_CALL(presentation_service_, StopListeningForScreenAvailability(gurl2_))
      .Times(0);
  EXPECT_CALL(presentation_service_, StopListeningForScreenAvailability(gurl3_))
      .Times(0);

  // Set up |availability_set_| and |listening_status_|
  base::RunLoop run_loop;
  for (auto& mock_observer : mock_observers_) {
    client()->getAvailability(
        mock_observer->urls(),
        base::MakeUnique<WebPresentationAvailabilityCallbacks>());
  }

  for (auto* mock_observer : mock_observers_)
    client()->startListening(mock_observer);

  EXPECT_CALL(mock_observer1_, availabilityChanged(false));
  EXPECT_CALL(mock_observer2_, availabilityChanged(false));
  EXPECT_CALL(mock_observer3_, availabilityChanged(false));

  // Clean up callbacks.
  ChangeURLState(gurl2_, URLState::Unavailable);
  client()->stopListening(&mock_observer1_);
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest,
       OnScreenAvailabilityUpdatedInvokesAvailabilityChanged) {
  for (const auto& gurl : gurls_) {
    EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl))
        .Times(1);
  }
  EXPECT_CALL(mock_observer1_, availabilityChanged(true));

  base::RunLoop run_loop;
  for (auto* mock_observer : mock_observers_) {
    client()->getAvailability(
        mock_observer->urls(),
        base::MakeUnique<WebPresentationAvailabilityCallbacks>());
    client()->startListening(mock_observer);
  }

  ChangeURLState(gurl1_, URLState::Available);
  run_loop.RunUntilIdle();

  EXPECT_CALL(mock_observer1_, availabilityChanged(false));

  base::RunLoop run_loop_2;
  ChangeURLState(gurl1_, URLState::Unavailable);
  run_loop_2.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest,
       OnScreenAvailabilityUpdatedInvokesMultipleAvailabilityChanged) {
  for (const auto& gurl : gurls_) {
    EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl))
        .Times(1);
  }
  for (auto* mock_observer : mock_observers_)
    EXPECT_CALL(*mock_observer, availabilityChanged(true));

  base::RunLoop run_loop;
  for (auto* mock_observer : mock_observers_) {
    client()->getAvailability(
        mock_observer->urls(),
        base::MakeUnique<WebPresentationAvailabilityCallbacks>());
    client()->startListening(mock_observer);
  }

  ChangeURLState(gurl2_, URLState::Available);
  run_loop.RunUntilIdle();

  for (auto* mock_observer : mock_observers_)
    EXPECT_CALL(*mock_observer, availabilityChanged(false));

  base::RunLoop run_loop_2;
  ChangeURLState(gurl2_, URLState::Unavailable);
  run_loop_2.RunUntilIdle();
}

}  // namespace content
