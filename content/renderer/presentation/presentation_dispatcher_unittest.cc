// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "content/public/common/presentation_connection_message.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/renderer/presentation/presentation_dispatcher.h"
#include "content/renderer/presentation/test_presentation_connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnection.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnectionCallbacks.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationError.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationInfo.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationReceiver.h"
#include "third_party/WebKit/public/web/WebArrayBuffer.h"

using ::testing::_;
using ::testing::Invoke;
using blink::WebArrayBuffer;
using blink::WebPresentationConnectionCallbacks;
using blink::WebPresentationError;
using blink::WebPresentationInfo;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;
using blink::mojom::PresentationConnection;
using blink::mojom::PresentationService;

namespace content {

class MockPresentationService : public PresentationService {
 public:
  void SetController(
      blink::mojom::PresentationControllerPtr controller) override {}
  void SetReceiver(blink::mojom::PresentationReceiverPtr receiver) override {}
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
                             const std::string& presentation_id,
                             ReconnectPresentationCallback callback) {
    ReconnectPresentationInternal(presentation_urls, presentation_id, callback);
  }
  MOCK_METHOD3(ReconnectPresentationInternal,
               void(const std::vector<GURL>& presentation_urls,
                    const std::string& presentation_id,
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
};

class TestPresentationReceiver : public blink::WebPresentationReceiver {
 public:
  MOCK_METHOD0(InitIfNeeded, void());
  MOCK_METHOD0(OnReceiverTerminated, void());
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
      mock_binding_ = std::make_unique<mojo::Binding<PresentationService>>(
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
        dispatcher_(&presentation_service_) {}

  ~PresentationDispatcherTest() override {}

  void SetUp() override {
    // Set some test data.
    *array_buffer_data() = 42;
  }

  uint8_t* array_buffer_data() {
    return static_cast<uint8_t*>(array_buffer_.Data());
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

  MockPresentationService presentation_service_;
  TestPresentationDispatcher dispatcher_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(PresentationDispatcherTest, TestStartPresentation) {
  TestPresentationConnection connection;
  {
    base::RunLoop run_loop;
    EXPECT_CALL(presentation_service_, StartPresentationInternal(gurls_, _))
        .WillOnce(Invoke(
            [this](const std::vector<GURL>& presentation_urls,
                   PresentationService::StartPresentationCallback& callback) {
              PresentationInfo presentation_info(gurl1_,
                                                 presentation_id_.Utf8());
              std::move(callback).Run(presentation_info, base::nullopt);
            }));

    EXPECT_CALL(connection, Init()).Times(1);
    dispatcher_.StartPresentation(
        urls_, std::make_unique<TestWebPresentationConnectionCallback>(
                   url1_, presentation_id_, &connection));
    run_loop.RunUntilIdle();
  }
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
      std::make_unique<TestWebPresentationConnectionErrorCallback>(
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
      std::make_unique<TestWebPresentationConnectionErrorCallback>(
          WebPresentationError::kErrorTypeNoAvailableScreens, error_message));
  run_loop.RunUntilIdle();
}

TEST_F(PresentationDispatcherTest, TestReconnectPresentation) {
  TestPresentationConnection connection;
  {
    base::RunLoop run_loop;
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

    EXPECT_CALL(connection, Init()).Times(1);
    dispatcher_.ReconnectPresentation(
        urls_, presentation_id_,
        std::make_unique<TestWebPresentationConnectionCallback>(
            url1_, presentation_id_, &connection));
    run_loop.RunUntilIdle();
  }
}

TEST_F(PresentationDispatcherTest, TestReconnectPresentationNoConnection) {
  TestPresentationConnection connection;
  {
    base::RunLoop run_loop;
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

    EXPECT_CALL(connection, Init()).Times(0);
    dispatcher_.ReconnectPresentation(
        urls_, presentation_id_,
        std::make_unique<TestWebPresentationConnectionCallback>(
            url1_, presentation_id_, nullptr));
    run_loop.RunUntilIdle();
  }
}

}  // namespace content
