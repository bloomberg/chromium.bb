// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "content/public/common/presentation_connection_message.h"
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
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationInfo.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationReceiver.h"
#include "third_party/WebKit/public/web/WebArrayBuffer.h"

using ::testing::_;
using ::testing::Invoke;
using blink::WebArrayBuffer;
using blink::WebPresentationAvailabilityCallbacks;
using blink::WebPresentationAvailabilityObserver;
using blink::WebPresentationConnectionCallbacks;
using blink::WebPresentationError;
using blink::WebPresentationInfo;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;
using blink::mojom::PresentationConnection;
using blink::mojom::PresentationService;
using blink::mojom::PresentationServiceClientPtr;
using blink::mojom::ScreenAvailability;

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

  MOCK_METHOD1(AvailabilityChanged, void(ScreenAvailability availability));
  const WebVector<WebURL>& Urls() const override { return urls_; }

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

  // TODO(crbug.com/729950): Use MOCK_METHOD directly once GMock gets the
  // move-only type support.
  void StartPresentation(const std::vector<GURL>& presentation_urls,
                         StartPresentationCallback callback) {
    StartPresentationInternal(presentation_urls, callback);
  }
  MOCK_METHOD2(StartPresentationInternal,
               void(const std::vector<GURL>& presentation_urls,
                    StartPresentationCallback& callback));

  void ReconnectPresentation(const std::vector<GURL>& presentation_urls,
                             const base::Optional<std::string>& presentation_id,
                             ReconnectPresentationCallback callback) {
    ReconnectPresentationInternal(presentation_urls, presentation_id, callback);
  }
  MOCK_METHOD3(ReconnectPresentationInternal,
               void(const std::vector<GURL>& presentation_urls,
                    const base::Optional<std::string>& presentation_id,
                    ReconnectPresentationCallback& callback));

  void SetPresentationConnection(
      const PresentationInfo& presentation_info,
      blink::mojom::PresentationConnectionPtr controller_conn_ptr,
      blink::mojom::PresentationConnectionRequest receiver_conn_request)
      override {
    SetPresentationConnection(presentation_info, controller_conn_ptr.get());
  }
  MOCK_METHOD2(SetPresentationConnection,
               void(const PresentationInfo& presentation_info,
                    PresentationConnection* connection));

  MOCK_METHOD2(CloseConnection,
               void(const GURL& presentation_url,
                    const std::string& presentation_id));
  MOCK_METHOD2(Terminate,
               void(const GURL& presentation_url,
                    const std::string& presentation_id));
  MOCK_METHOD1(ListenForConnectionMessages,
               void(const PresentationInfo& presentation_info));
};

class TestPresentationConnectionProxy : public PresentationConnectionProxy {
 public:
  TestPresentationConnectionProxy(blink::WebPresentationConnection* connection)
      : PresentationConnectionProxy(connection) {}

  // PresentationConnectionMessage is move-only.
  void SendConnectionMessage(PresentationConnectionMessage message,
                             OnMessageCallback cb) const {
    SendConnectionMessageInternal(message, cb);
  }
  MOCK_CONST_METHOD2(SendConnectionMessageInternal,
                     void(const PresentationConnectionMessage&,
                          OnMessageCallback&));
  MOCK_CONST_METHOD0(Close, void());
};

class TestPresentationReceiver : public blink::WebPresentationReceiver {
 public:
  blink::WebPresentationConnection* OnReceiverConnectionAvailable(
      const blink::WebPresentationInfo&) override {
    return &connection_;
  }

  MOCK_METHOD1(DidChangeConnectionState,
               void(blink::WebPresentationConnectionState));
  MOCK_METHOD0(TerminateConnection, void());
  MOCK_METHOD1(RemoveConnection, void(blink::WebPresentationConnection*));

  TestPresentationConnection connection_;
};

class MockPresentationAvailabilityCallbacks
    : public blink::WebCallbacks<bool, const blink::WebPresentationError&> {
 public:
  MOCK_METHOD1(OnSuccess, void(bool value));
  MOCK_METHOD1(OnError, void(const blink::WebPresentationError&));
};

class TestWebPresentationConnectionCallback
    : public WebPresentationConnectionCallbacks {
 public:
  // Does not take ownership of |connection|.
  TestWebPresentationConnectionCallback(WebURL url,
                                        WebString id,
                                        TestPresentationConnection* connection)
      : url_(url), id_(id), callback_called_(false), connection_(connection) {}
  ~TestWebPresentationConnectionCallback() override {
    EXPECT_TRUE(callback_called_);
  }

  void OnSuccess(const WebPresentationInfo& info) override {
    callback_called_ = true;
    EXPECT_EQ(info.url, url_);
    EXPECT_EQ(info.id, id_);
  }

  blink::WebPresentationConnection* GetConnection() override {
    return connection_;
  }

 private:
  const WebURL url_;
  const WebString id_;
  bool callback_called_;
  TestPresentationConnection* connection_;
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

  void OnError(const WebPresentationError& error) override {
    callback_called_ = true;
    EXPECT_EQ(error.error_type, error_type_);
    EXPECT_EQ(error.message, message_);
  }

  blink::WebPresentationConnection* GetConnection() override { return nullptr; }

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
  using OnMessageCallback = PresentationConnectionProxy::OnMessageCallback;

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
        presentation_id_(WebString::FromUTF8("test-id")),
        array_buffer_(WebArrayBuffer::Create(4, 1)),
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
    return static_cast<uint8_t*>(array_buffer_.Data());
  }

  void ChangeURLState(const GURL& url, ScreenAvailability state) {
    switch (state) {
      case ScreenAvailability::AVAILABLE:
        dispatcher_.OnScreenAvailabilityUpdated(url,
                                                ScreenAvailability::AVAILABLE);
        break;
      case ScreenAvailability::SOURCE_NOT_SUPPORTED:
        dispatcher_.OnScreenAvailabilityUpdated(
            url, ScreenAvailability::SOURCE_NOT_SUPPORTED);
        break;
      case ScreenAvailability::UNAVAILABLE:
        dispatcher_.OnScreenAvailabilityUpdated(
            url, ScreenAvailability::UNAVAILABLE);
        break;
      case ScreenAvailability::DISABLED:
        dispatcher_.OnScreenAvailabilityUpdated(url,
                                                ScreenAvailability::DISABLED);
        break;
      case ScreenAvailability::UNKNOWN:
        break;
    }
  }

  // Tests that PresenationService is called for getAvailability(urls), after
  // |urls| change state to |states|. This function takes ownership of
  // |mock_callback|.
  void TestGetAvailability(
      const std::vector<GURL>& urls,
      const std::vector<ScreenAvailability>& states,
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
    client()->GetAvailability(urls, base::WrapUnique(mock_callback));
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

TEST_F(PresentationDispatcherTest, TestStartPresentation) {
  TestPresentationConnection connection;
  EXPECT_FALSE(connection.proxy());
  {
    base::RunLoop run_loop;
    EXPECT_CALL(presentation_service_, ListenForConnectionMessages(_));
    EXPECT_CALL(presentation_service_, SetPresentationConnection(_, _));
    EXPECT_CALL(presentation_service_, StartPresentationInternal(gurls_, _))
        .WillOnce(Invoke(
            [this](const std::vector<GURL>& presentation_urls,
                   PresentationService::StartPresentationCallback& callback) {
              PresentationInfo presentation_info(gurl1_,
                                                 presentation_id_.Utf8());
              std::move(callback).Run(presentation_info, base::nullopt);
            }));

    dispatcher_.StartPresentation(
        urls_, base::MakeUnique<TestWebPresentationConnectionCallback>(
                   url1_, presentation_id_, &connection));
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(connection.proxy());
}

TEST_F(PresentationDispatcherTest, TestStartPresentationError) {
  WebString error_message = WebString::FromUTF8("Test error message");
  base::RunLoop run_loop;

  EXPECT_CALL(presentation_service_, StartPresentationInternal(gurls_, _))
      .WillOnce(
          Invoke([&error_message](
                     const std::vector<GURL>& presentation_urls,
                     PresentationService::StartPresentationCallback& callback) {
            std::move(callback).Run(
                base::nullopt,
                PresentationError(
                    content::PRESENTATION_ERROR_NO_AVAILABLE_SCREENS,
                    error_message.Utf8()));
          }));
  dispatcher_.StartPresentation(
      urls_,
      base::MakeUnique<TestWebPresentationConnectionErrorCallback>(
          WebPresentationError::kErrorTypeNoAvailableScreens, error_message));
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestReconnectPresentationError) {
  WebString error_message = WebString::FromUTF8("Test error message");
  base::RunLoop run_loop;

  EXPECT_CALL(presentation_service_,
              ReconnectPresentationInternal(gurls_, _, _))
      .WillOnce(Invoke(
          [this, &error_message](
              const std::vector<GURL>& presentation_urls,
              const base::Optional<std::string>& presentation_id,
              PresentationService::ReconnectPresentationCallback& callback) {
            EXPECT_TRUE(presentation_id.has_value());
            EXPECT_EQ(presentation_id_.Utf8(), presentation_id.value());
            std::move(callback).Run(
                base::nullopt,
                PresentationError(
                    content::PRESENTATION_ERROR_NO_AVAILABLE_SCREENS,
                    error_message.Utf8()));
          }));
  dispatcher_.ReconnectPresentation(
      urls_, presentation_id_,
      base::MakeUnique<TestWebPresentationConnectionErrorCallback>(
          WebPresentationError::kErrorTypeNoAvailableScreens, error_message));
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestReconnectPresentation) {
  TestPresentationConnection connection;
  EXPECT_FALSE(connection.proxy());
  {
    base::RunLoop run_loop;
    EXPECT_CALL(presentation_service_, ListenForConnectionMessages(_));
    EXPECT_CALL(presentation_service_, SetPresentationConnection(_, _));
    EXPECT_CALL(presentation_service_,
                ReconnectPresentationInternal(gurls_, _, _))
        .WillOnce(Invoke(
            [this](
                const std::vector<GURL>& presentation_urls,
                const base::Optional<std::string>& presentation_id,
                PresentationService::ReconnectPresentationCallback& callback) {
              EXPECT_TRUE(presentation_id.has_value());
              EXPECT_EQ(presentation_id_.Utf8(), presentation_id.value());
              std::move(callback).Run(
                  PresentationInfo(gurl1_, presentation_id_.Utf8()),
                  base::nullopt);
            }));

    dispatcher_.ReconnectPresentation(
        urls_, presentation_id_,
        base::MakeUnique<TestWebPresentationConnectionCallback>(
            url1_, presentation_id_, &connection));
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(connection.proxy());
}

TEST_F(PresentationDispatcherTest, TestSendString) {
  WebString message = WebString::FromUTF8("test message");
  TestPresentationConnection connection;
  TestPresentationConnectionProxy connection_proxy(&connection);

  PresentationConnectionMessage expected_message(message.Utf8());

  base::RunLoop run_loop;
  EXPECT_CALL(connection_proxy, SendConnectionMessageInternal(_, _))
      .WillOnce(Invoke([&expected_message](
                           const PresentationConnectionMessage& message_request,
                           OnMessageCallback& callback) {
        EXPECT_EQ(message_request, expected_message);
        std::move(callback).Run(true);
      }));

  dispatcher_.SendString(url1_, presentation_id_, message, &connection_proxy);
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestSendArrayBuffer) {
  std::vector<uint8_t> data(array_buffer_data(),
                            array_buffer_data() + array_buffer_.ByteLength());
  TestPresentationConnection connection;
  TestPresentationConnectionProxy connection_proxy(&connection);
  PresentationConnectionMessage expected_message(data);

  base::RunLoop run_loop;
  EXPECT_CALL(connection_proxy, SendConnectionMessageInternal(_, _))
      .WillOnce(Invoke([&expected_message](
                           const PresentationConnectionMessage& message_request,
                           OnMessageCallback& callback) {
        EXPECT_EQ(message_request, expected_message);
        std::move(callback).Run(true);
      }));
  dispatcher_.SendArrayBuffer(url1_, presentation_id_, array_buffer_data(),
                              array_buffer_.ByteLength(), &connection_proxy);
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestSendBlobData) {
  std::vector<uint8_t> data(array_buffer_data(),
                            array_buffer_data() + array_buffer_.ByteLength());
  TestPresentationConnection connection;
  TestPresentationConnectionProxy connection_proxy(&connection);
  PresentationConnectionMessage expected_message(data);

  base::RunLoop run_loop;
  EXPECT_CALL(connection_proxy, SendConnectionMessageInternal(_, _))
      .WillOnce(Invoke([&expected_message](
                           const PresentationConnectionMessage& message_request,
                           OnMessageCallback& callback) {
        EXPECT_EQ(message_request, expected_message);
        std::move(callback).Run(true);
      }));
  dispatcher_.SendBlobData(url1_, presentation_id_, array_buffer_data(),
                           array_buffer_.ByteLength(), &connection_proxy);
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestOnReceiverConnectionAvailable) {
  PresentationInfo presentation_info(gurl1_, presentation_id_.Utf8());

  blink::mojom::PresentationConnectionPtr controller_connection_ptr;
  TestPresentationConnection controller_connection;
  TestPresentationConnectionProxy controller_connection_proxy(
      &controller_connection);
  mojo::Binding<blink::mojom::PresentationConnection> binding(
      &controller_connection_proxy,
      mojo::MakeRequest(&controller_connection_ptr));

  blink::mojom::PresentationConnectionPtr receiver_connection_ptr;

  TestPresentationReceiver receiver;
  dispatcher_.SetReceiver(&receiver);

  base::RunLoop run_loop;
  EXPECT_CALL(
      controller_connection,
      DidChangeState(blink::WebPresentationConnectionState::kConnected));
  EXPECT_CALL(
      receiver.connection_,
      DidChangeState(blink::WebPresentationConnectionState::kConnected));

  dispatcher_.OnReceiverConnectionAvailable(
      std::move(presentation_info), std::move(controller_connection_ptr),
      mojo::MakeRequest(&receiver_connection_ptr));

  EXPECT_TRUE(receiver_connection_ptr);
  EXPECT_TRUE(receiver.connection_.proxy());
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestCloseConnection) {
  base::RunLoop run_loop;
  TestPresentationConnection connection;
  TestPresentationConnectionProxy test_proxy(&connection);
  EXPECT_CALL(test_proxy, Close());
  EXPECT_CALL(presentation_service_,
              CloseConnection(gurl1_, presentation_id_.Utf8()));
  dispatcher_.CloseConnection(url1_, presentation_id_, &test_proxy);
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestTerminatePresentation) {
  base::RunLoop run_loop;
  EXPECT_CALL(presentation_service_,
              Terminate(gurl1_, presentation_id_.Utf8()));
  dispatcher_.TerminatePresentation(url1_, presentation_id_);
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestListenForScreenAvailability) {
  base::RunLoop run_loop1;
  for (const auto& gurl : gurls_) {
    EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl));
    EXPECT_CALL(presentation_service_,
                StopListeningForScreenAvailability(gurl));
  }

  dispatcher_.GetAvailability(
      urls_, base::MakeUnique<WebPresentationAvailabilityCallbacks>());
  dispatcher_.OnScreenAvailabilityUpdated(url1_, ScreenAvailability::AVAILABLE);
  run_loop1.RunUntilIdle();

  base::RunLoop run_loop2;
  for (const auto& gurl : gurls_)
    EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl));

  client()->StartListening(&observer_);
  run_loop2.RunUntilIdle();

  base::RunLoop run_loop3;
  EXPECT_CALL(observer_, AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  dispatcher_.OnScreenAvailabilityUpdated(url1_,
                                          ScreenAvailability::UNAVAILABLE);
  EXPECT_CALL(observer_,
              AvailabilityChanged(ScreenAvailability::SOURCE_NOT_SUPPORTED));
  dispatcher_.OnScreenAvailabilityUpdated(
      url1_, ScreenAvailability::SOURCE_NOT_SUPPORTED);
  EXPECT_CALL(observer_, AvailabilityChanged(ScreenAvailability::AVAILABLE));
  dispatcher_.OnScreenAvailabilityUpdated(url1_, ScreenAvailability::AVAILABLE);
  for (const auto& gurl : gurls_) {
    EXPECT_CALL(presentation_service_,
                StopListeningForScreenAvailability(gurl));
  }
  client()->StopListening(&observer_);
  run_loop3.RunUntilIdle();

  // After stopListening(), |observer_| should no longer be notified.
  base::RunLoop run_loop4;
  EXPECT_CALL(observer_, AvailabilityChanged(ScreenAvailability::UNAVAILABLE))
      .Times(0);
  dispatcher_.OnScreenAvailabilityUpdated(url1_,
                                          ScreenAvailability::UNAVAILABLE);
  run_loop4.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestSetDefaultPresentationUrls) {
  base::RunLoop run_loop;
  EXPECT_CALL(presentation_service_, SetDefaultPresentationUrls(gurls_));
  dispatcher_.SetDefaultPresentationUrls(urls_);
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, GetAvailabilityOneUrlNoAvailabilityChange) {
  auto* mock_callback =
      new testing::StrictMock<MockPresentationAvailabilityCallbacks>();

  EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl1_))
      .Times(1);

  base::RunLoop run_loop;
  client()->GetAvailability(std::vector<GURL>({gurl1_}),
                            base::WrapUnique(mock_callback));
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, GetAvailabilityOneUrlBecomesAvailable) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, OnSuccess(true));

  TestGetAvailability({url1_}, {ScreenAvailability::AVAILABLE}, mock_callback);
}

TEST_F(PresentationDispatcherTest, GetAvailabilityOneUrlBecomesNotCompatible) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, OnSuccess(false));

  TestGetAvailability({url1_}, {ScreenAvailability::SOURCE_NOT_SUPPORTED},
                      mock_callback);
}

TEST_F(PresentationDispatcherTest, GetAvailabilityOneUrlBecomesUnavailable) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, OnSuccess(false));

  TestGetAvailability({url1_}, {ScreenAvailability::UNAVAILABLE},
                      mock_callback);
}

TEST_F(PresentationDispatcherTest, GetAvailabilityOneUrlBecomesUnsupported) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, OnError(_));

  TestGetAvailability({url1_}, {ScreenAvailability::DISABLED}, mock_callback);
}

TEST_F(PresentationDispatcherTest,
       GetAvailabilityMultipleUrlsAllBecomesAvailable) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, OnSuccess(true)).Times(1);

  TestGetAvailability(
      {url1_, url2_},
      {ScreenAvailability::AVAILABLE, ScreenAvailability::AVAILABLE},
      mock_callback);
}

TEST_F(PresentationDispatcherTest,
       GetAvailabilityMultipleUrlsAllBecomesUnavailable) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, OnSuccess(false)).Times(1);

  TestGetAvailability(
      {url1_, url2_},
      {ScreenAvailability::UNAVAILABLE, ScreenAvailability::UNAVAILABLE},
      mock_callback);
}

TEST_F(PresentationDispatcherTest,
       GetAvailabilityMultipleUrlsAllBecomesNotCompatible) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, OnSuccess(false)).Times(1);

  TestGetAvailability({url1_, url2_},
                      {ScreenAvailability::SOURCE_NOT_SUPPORTED,
                       ScreenAvailability::SOURCE_NOT_SUPPORTED},
                      mock_callback);
}

TEST_F(PresentationDispatcherTest,
       GetAvailabilityMultipleUrlsAllBecomesUnsupported) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, OnError(_)).Times(1);

  TestGetAvailability(
      {url1_, url2_},
      {ScreenAvailability::DISABLED, ScreenAvailability::DISABLED},
      mock_callback);
}

TEST_F(PresentationDispatcherTest,
       GetAvailabilityReturnsDirectlyForAlreadyListeningUrls) {
  // First getAvailability() call.
  auto* mock_callback_1 = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback_1, OnSuccess(false)).Times(1);

  std::vector<ScreenAvailability> state_seq = {ScreenAvailability::UNAVAILABLE,
                                               ScreenAvailability::AVAILABLE,
                                               ScreenAvailability::UNAVAILABLE};
  TestGetAvailability({url1_, url2_, url3_}, state_seq, mock_callback_1);

  // Second getAvailability() call.
  for (const auto& url : mock_observer3_.Urls()) {
    EXPECT_CALL(presentation_service_, ListenForScreenAvailability((GURL)url))
        .Times(1);
  }
  auto* mock_callback_2 = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback_2, OnSuccess(true)).Times(1);

  base::RunLoop run_loop;
  client()->GetAvailability(mock_observer3_.Urls(),
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
    client()->GetAvailability(
        mock_observer->Urls(),
        base::MakeUnique<WebPresentationAvailabilityCallbacks>());
    client()->StartListening(mock_observer);
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

  EXPECT_CALL(mock_observer1_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  EXPECT_CALL(mock_observer2_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  EXPECT_CALL(mock_observer3_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));

  // Set up |availability_set_| and |listening_status_|
  base::RunLoop run_loop;
  for (auto* mock_observer : mock_observers_) {
    client()->GetAvailability(
        mock_observer->Urls(),
        base::MakeUnique<WebPresentationAvailabilityCallbacks>());

    client()->StartListening(mock_observer);
  }

  // Clean up callbacks.
  ChangeURLState(gurl2_, ScreenAvailability::UNAVAILABLE);

  for (auto* mock_observer : mock_observers_)
    client()->StopListening(mock_observer);

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
  for (auto* mock_observer : mock_observers_) {
    client()->GetAvailability(
        mock_observer->Urls(),
        base::MakeUnique<WebPresentationAvailabilityCallbacks>());
  }

  for (auto* mock_observer : mock_observers_)
    client()->StartListening(mock_observer);

  EXPECT_CALL(mock_observer1_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  EXPECT_CALL(mock_observer2_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  EXPECT_CALL(mock_observer3_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));

  // Clean up callbacks.
  ChangeURLState(gurl2_, ScreenAvailability::UNAVAILABLE);
  client()->StopListening(&mock_observer1_);
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest,
       OnScreenAvailabilityUpdatedInvokesAvailabilityChanged) {
  for (const auto& gurl : gurls_) {
    EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl))
        .Times(1);
  }
  EXPECT_CALL(mock_observer1_,
              AvailabilityChanged(ScreenAvailability::AVAILABLE));

  base::RunLoop run_loop;
  for (auto* mock_observer : mock_observers_) {
    client()->GetAvailability(
        mock_observer->Urls(),
        base::MakeUnique<WebPresentationAvailabilityCallbacks>());
    client()->StartListening(mock_observer);
  }

  ChangeURLState(gurl1_, ScreenAvailability::AVAILABLE);
  run_loop.RunUntilIdle();

  EXPECT_CALL(mock_observer1_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));

  base::RunLoop run_loop_2;
  ChangeURLState(gurl1_, ScreenAvailability::UNAVAILABLE);
  run_loop_2.RunUntilIdle();

  EXPECT_CALL(mock_observer1_,
              AvailabilityChanged(ScreenAvailability::SOURCE_NOT_SUPPORTED));

  base::RunLoop run_loop_3;
  ChangeURLState(gurl1_, ScreenAvailability::SOURCE_NOT_SUPPORTED);
  run_loop_3.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest,
       OnScreenAvailabilityUpdatedInvokesMultipleAvailabilityChanged) {
  for (const auto& gurl : gurls_) {
    EXPECT_CALL(presentation_service_, ListenForScreenAvailability(gurl))
        .Times(1);
  }
  for (auto* mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::AVAILABLE));
  }

  base::RunLoop run_loop;
  for (auto* mock_observer : mock_observers_) {
    client()->GetAvailability(
        mock_observer->Urls(),
        base::MakeUnique<WebPresentationAvailabilityCallbacks>());
    client()->StartListening(mock_observer);
  }

  ChangeURLState(gurl2_, ScreenAvailability::AVAILABLE);
  run_loop.RunUntilIdle();

  for (auto* mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  }

  base::RunLoop run_loop_2;
  ChangeURLState(gurl2_, ScreenAvailability::UNAVAILABLE);
  run_loop_2.RunUntilIdle();

  for (auto* mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::SOURCE_NOT_SUPPORTED));
  }

  base::RunLoop run_loop_3;
  ChangeURLState(gurl2_, ScreenAvailability::SOURCE_NOT_SUPPORTED);
  run_loop_3.RunUntilIdle();
}

}  // namespace content
