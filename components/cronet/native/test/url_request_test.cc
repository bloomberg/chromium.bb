// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "components/cronet/native/test/test_url_request_callback.h"
#include "components/cronet/native/test/test_util.h"
#include "components/cronet/test/test_server.h"
#include "cronet_c.h"
#include "net/cert/mock_cert_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class UrlRequestTest : public ::testing::Test {
 protected:
  UrlRequestTest() {}
  ~UrlRequestTest() override {}

  void SetUp() override { cronet::TestServer::Start(); }

  void TearDown() override { cronet::TestServer::Shutdown(); }

  std::unique_ptr<cronet::test::TestUrlRequestCallback> StartAndWaitForComplete(
      const std::string& url) {
    auto test_callback =
        std::make_unique<cronet::test::TestUrlRequestCallback>();

    Cronet_EnginePtr engine = cronet::test::CreateTestEngine(0);
    Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
    Cronet_UrlRequestParamsPtr request_params =
        Cronet_UrlRequestParams_Create();
    // Executor provided by the application.
    Cronet_ExecutorPtr executor = test_callback->CreateExecutor(false);
    // Callback provided by the application.
    Cronet_UrlRequestCallbackPtr callback =
        test_callback->CreateUrlRequestCallback();

    Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                     request_params, callback, executor);

    Cronet_UrlRequest_Start(request);
    test_callback->WaitForDone();
    // Wait for all posted tasks to be executed to ensure there is no unhandled
    // exception.
    test_callback->ShutdownExecutor();
    EXPECT_TRUE(test_callback->IsDone());
    EXPECT_TRUE(Cronet_UrlRequest_IsDone(request));

    Cronet_UrlRequestParams_Destroy(request_params);
    Cronet_UrlRequest_Destroy(request);
    Cronet_UrlRequestCallback_Destroy(callback);
    Cronet_Executor_Destroy(executor);
    Cronet_Engine_Destroy(engine);
    return test_callback;
  }

  void CheckResponseInfo(
      const cronet::test::TestUrlRequestCallback::UrlResponseInfo&
          response_info,
      const std::string& expected_url,
      int expected_http_status_code,
      const std::string& expected_http_status_text) {
    EXPECT_EQ(expected_url, response_info.url);
    EXPECT_EQ(expected_url, response_info.url_chain.back());
    EXPECT_EQ(expected_http_status_code, response_info.http_status_code);
    EXPECT_EQ(expected_http_status_text, response_info.http_status_text);
    EXPECT_FALSE(response_info.was_cached);
  }

  void CheckResponseInfoHeader(
      const cronet::test::TestUrlRequestCallback::UrlResponseInfo&
          response_info,
      const std::string& header_name,
      const std::string& header_value) {
    for (const auto& header : response_info.all_headers) {
      if (header.first == header_name) {
        EXPECT_EQ(header.second, header_value);
        return;
      }
    }
    NOTREACHED();
  }

  void ExpectResponseInfoEquals(
      const cronet::test::TestUrlRequestCallback::UrlResponseInfo& expected,
      const cronet::test::TestUrlRequestCallback::UrlResponseInfo& actual) {
    EXPECT_EQ(expected.url, actual.url);
    EXPECT_EQ(expected.url_chain, actual.url_chain);
    EXPECT_EQ(expected.http_status_code, actual.http_status_code);
    EXPECT_EQ(expected.http_status_text, actual.http_status_text);
    EXPECT_EQ(expected.all_headers, actual.all_headers);
    EXPECT_EQ(expected.was_cached, actual.was_cached);
    EXPECT_EQ(expected.negotiated_protocol, actual.negotiated_protocol);
    EXPECT_EQ(expected.proxy_server, actual.proxy_server);
    EXPECT_EQ(expected.received_byte_count, actual.received_byte_count);
  }

 protected:
  // Provide a message loop for use by TestExecutor instances.
  base::MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UrlRequestTest);
};

TEST_F(UrlRequestTest, InitChecks) {
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_EnginePtr engine = Cronet_Engine_Create();
  // Disable runtime CHECK of the result, so it could be verified.
  Cronet_EngineParams_enable_check_result_set(engine_params, false);
  EXPECT_EQ(Cronet_RESULT_SUCCESS,
            Cronet_Engine_StartWithParams(engine, engine_params));

  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  const std::string url = cronet::TestServer::GetEchoMethodURL();

  cronet::test::TestUrlRequestCallback test_callback;
  // Executor provided by the application.
  Cronet_ExecutorPtr executor = test_callback.CreateExecutor(false);
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback =
      test_callback.CreateUrlRequestCallback();
  EXPECT_EQ(Cronet_RESULT_NULL_POINTER_URL,
            Cronet_UrlRequest_InitWithParams(
                request, engine, /* url = */ nullptr,
                /* request_params = */ nullptr, /* callback = */ nullptr,
                /* executor = */ nullptr));
  EXPECT_EQ(Cronet_RESULT_NULL_POINTER_PARAMS,
            Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                             /* request_params = */ nullptr,
                                             /* callback = */ nullptr,
                                             /* executor = */ nullptr));
  EXPECT_EQ(Cronet_RESULT_NULL_POINTER_CALLBACK,
            Cronet_UrlRequest_InitWithParams(
                request, engine, url.c_str(), request_params,
                /* callback = */ nullptr, /* executor = */ nullptr));
  EXPECT_EQ(Cronet_RESULT_NULL_POINTER_EXECUTOR,
            Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                             request_params, callback,
                                             /* executor = */ nullptr));
  EXPECT_EQ(Cronet_RESULT_NULL_POINTER_EXECUTOR,
            Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                             request_params, callback,
                                             /* executor = */ nullptr));
  Cronet_UrlRequestParams_http_method_set(request_params, "bad:method");
  EXPECT_EQ(
      Cronet_RESULT_ILLEGAL_ARGUMENT_INVALID_HTTP_METHOD,
      Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                       request_params, callback, executor));
  Cronet_UrlRequestParams_http_method_set(request_params, "HEAD");
  // Check header validation
  Cronet_HttpHeaderPtr http_header = Cronet_HttpHeader_Create();
  Cronet_UrlRequestParams_request_headers_add(request_params, http_header);
  EXPECT_EQ(
      Cronet_RESULT_NULL_POINTER_HEADER_NAME,
      Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                       request_params, callback, executor));
  Cronet_UrlRequestParams_request_headers_clear(request_params);

  Cronet_HttpHeader_name_set(http_header, "bad:name");
  Cronet_UrlRequestParams_request_headers_add(request_params, http_header);
  EXPECT_EQ(
      Cronet_RESULT_NULL_POINTER_HEADER_VALUE,
      Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                       request_params, callback, executor));
  Cronet_UrlRequestParams_request_headers_clear(request_params);

  Cronet_HttpHeader_value_set(http_header, "header value");
  Cronet_UrlRequestParams_request_headers_add(request_params, http_header);
  EXPECT_EQ(
      Cronet_RESULT_ILLEGAL_ARGUMENT_INVALID_HTTP_HEADER,
      Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                       request_params, callback, executor));
  Cronet_UrlRequestParams_request_headers_clear(request_params);

  Cronet_HttpHeader_name_set(http_header, "header-name");
  Cronet_UrlRequestParams_request_headers_add(request_params, http_header);
  EXPECT_EQ(Cronet_RESULT_SUCCESS, Cronet_UrlRequest_InitWithParams(
                                       request, engine, url.c_str(),
                                       request_params, callback, executor));
  Cronet_HttpHeader_Destroy(http_header);
  Cronet_UrlRequest_Destroy(request);
  Cronet_UrlRequestParams_Destroy(request_params);
  Cronet_UrlRequestCallback_Destroy(callback);
  Cronet_Executor_Destroy(executor);
  Cronet_Engine_Destroy(engine);
}

TEST_F(UrlRequestTest, SimpleGet) {
  const std::string url = cronet::TestServer::GetEchoMethodURL();
  auto callback = StartAndWaitForComplete(url);
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  // Default method is 'GET'.
  EXPECT_EQ("GET", callback->response_as_string_);
  EXPECT_EQ(0, callback->redirect_count_);
  EXPECT_EQ(callback->response_step_, callback->ON_SUCCEEDED);
  CheckResponseInfo(*callback->response_info_, url, 200, "OK");
  cronet::test::TestUrlRequestCallback::UrlResponseInfo expected_response_info(
      std::vector<std::string>({url}), "OK", 200, 86,
      std::vector<std::string>({"Connection", "close", "Content-Length", "3",
                                "Content-Type", "text/plain"}));
  ExpectResponseInfoEquals(expected_response_info, *callback->response_info_);
}

TEST_F(UrlRequestTest, SimpleRequest) {
  Cronet_EnginePtr engine = cronet::test::CreateTestEngine(0);
  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  std::string url = cronet::TestServer::GetSimpleURL();

  cronet::test::TestUrlRequestCallback test_callback;
  // Executor provided by the application.
  Cronet_ExecutorPtr executor = test_callback.CreateExecutor(false);
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback =
      test_callback.CreateUrlRequestCallback();

  Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(), request_params,
                                   callback, executor);

  Cronet_UrlRequest_Start(request);

  test_callback.WaitForDone();
  EXPECT_TRUE(test_callback.IsDone());
  ASSERT_EQ("The quick brown fox jumps over the lazy dog.",
            test_callback.response_as_string_);

  Cronet_UrlRequestParams_Destroy(request_params);
  Cronet_UrlRequest_Destroy(request);
  Cronet_UrlRequestCallback_Destroy(callback);
  Cronet_Executor_Destroy(executor);
  Cronet_Engine_Destroy(engine);
}

TEST_F(UrlRequestTest, MultiRedirect) {
  const std::string url = cronet::TestServer::GetMultiRedirectURL();
  auto callback = StartAndWaitForComplete(url);
  EXPECT_EQ(2, callback->redirect_count_);
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  EXPECT_EQ(2ul, callback->redirect_response_info_list_.size());

  // Check first redirect (multiredirect.html -> redirect.html).
  cronet::test::TestUrlRequestCallback::UrlResponseInfo
      first_expected_response_info(
          std::vector<std::string>({url}), "Found", 302, 76,
          std::vector<std::string>(
              {"Location", GURL(cronet::TestServer::GetRedirectURL()).path(),
               "redirect-header0", "header-value"}));
  ExpectResponseInfoEquals(first_expected_response_info,
                           *callback->redirect_response_info_list_.front());

  // Check second redirect (redirect.html -> success.txt).
  cronet::test::TestUrlRequestCallback::UrlResponseInfo
      second_expected_response_info(
          std::vector<std::string>({cronet::TestServer::GetMultiRedirectURL(),
                                    cronet::TestServer::GetRedirectURL()}),
          "Found", 302, 149,
          std::vector<std::string>(
              {"Location", GURL(cronet::TestServer::GetSuccessURL()).path(),
               "redirect-header", "header-value"}));
  ExpectResponseInfoEquals(second_expected_response_info,
                           *callback->redirect_response_info_list_.back());
  // Check final response (success.txt).
  cronet::test::TestUrlRequestCallback::UrlResponseInfo
      final_expected_response_info(
          std::vector<std::string>({cronet::TestServer::GetMultiRedirectURL(),
                                    cronet::TestServer::GetRedirectURL(),
                                    cronet::TestServer::GetSuccessURL()}),
          "OK", 200, 334,
          std::vector<std::string>(
              {"Content-Type", "text/plain", "Access-Control-Allow-Origin", "*",
               "header-name", "header-value", "multi-header-name",
               "header-value1", "multi-header-name", "header-value2"}));
  ExpectResponseInfoEquals(final_expected_response_info,
                           *callback->response_info_);
  EXPECT_NE(0, callback->response_data_length_);
  EXPECT_EQ(callback->ON_SUCCEEDED, callback->response_step_);
}

TEST_F(UrlRequestTest, CancelRequest) {
  Cronet_EnginePtr engine = cronet::test::CreateTestEngine(0);
  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  std::string url = cronet::TestServer::GetSimpleURL();

  cronet::test::TestUrlRequestCallback test_callback;
  test_callback.set_failure(test_callback.CANCEL_SYNC,
                            test_callback.ON_RESPONSE_STARTED);
  // Executor provided by the application.
  Cronet_ExecutorPtr executor = test_callback.CreateExecutor(false);
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback =
      test_callback.CreateUrlRequestCallback();

  Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(), request_params,
                                   callback, executor);

  Cronet_UrlRequest_Start(request);

  test_callback.WaitForDone();
  EXPECT_TRUE(test_callback.IsDone());
  EXPECT_TRUE(test_callback.on_canceled_called_);
  ASSERT_FALSE(test_callback.on_error_called_);
  EXPECT_TRUE(test_callback.response_as_string_.empty());

  Cronet_UrlRequestParams_Destroy(request_params);
  Cronet_UrlRequest_Destroy(request);
  Cronet_UrlRequestCallback_Destroy(callback);
  Cronet_Executor_Destroy(executor);
  Cronet_Engine_Destroy(engine);
}

TEST_F(UrlRequestTest, FailedRequestHostNotFound) {
  Cronet_EnginePtr engine = cronet::test::CreateTestEngine(0);
  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  std::string url = "https://notfound.example.com";

  cronet::test::TestUrlRequestCallback test_callback;
  // Executor provided by the application.
  Cronet_ExecutorPtr executor = test_callback.CreateExecutor(false);
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback =
      test_callback.CreateUrlRequestCallback();

  Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(), request_params,
                                   callback, executor);

  Cronet_UrlRequest_Start(request);

  test_callback.WaitForDone();
  EXPECT_TRUE(test_callback.IsDone());
  EXPECT_TRUE(test_callback.on_error_called_);
  ASSERT_FALSE(test_callback.on_canceled_called_);

  EXPECT_TRUE(test_callback.response_as_string_.empty());
  ASSERT_EQ(nullptr, test_callback.response_info_);
  ASSERT_NE(nullptr, test_callback.last_error_);

  ASSERT_EQ(Cronet_Error_ERROR_CODE_ERROR_HOSTNAME_NOT_RESOLVED,
            Cronet_Error_error_code_get(test_callback.last_error_));
  ASSERT_FALSE(
      Cronet_Error_immediately_retryable_get(test_callback.last_error_));
  ASSERT_STREQ("net::ERR_NAME_NOT_RESOLVED",
               Cronet_Error_message_get(test_callback.last_error_));
  ASSERT_EQ(-105,
            Cronet_Error_internal_error_code_get(test_callback.last_error_));
  ASSERT_EQ(
      0, Cronet_Error_quic_detailed_error_code_get(test_callback.last_error_));

  Cronet_UrlRequestParams_Destroy(request_params);
  Cronet_UrlRequest_Destroy(request);
  Cronet_UrlRequestCallback_Destroy(callback);
  Cronet_Executor_Destroy(executor);
  Cronet_Engine_Destroy(engine);
}

TEST_F(UrlRequestTest, PerfTest) {
  const int kTestIterations = 10;
  const int kDownloadSize = 19307439;  // used for internal server only

  Cronet_EnginePtr engine = Cronet_Engine_Create();
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_Engine_StartWithParams(engine, engine_params);

  std::string url = cronet::TestServer::PrepareBigDataURL(kDownloadSize);

  base::Time start = base::Time::Now();

  for (int i = 0; i < kTestIterations; ++i) {
    Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
    Cronet_UrlRequestParamsPtr request_params =
        Cronet_UrlRequestParams_Create();
    cronet::test::TestUrlRequestCallback test_callback;
    test_callback.set_accumulate_response_data(false);
    // Executor provided by the application.
    Cronet_ExecutorPtr executor = test_callback.CreateExecutor(false);
    // Callback provided by the application.
    Cronet_UrlRequestCallbackPtr callback =
        test_callback.CreateUrlRequestCallback();

    Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                     request_params, callback, executor);

    Cronet_UrlRequest_Start(request);
    test_callback.WaitForDone();

    EXPECT_TRUE(test_callback.IsDone());
    ASSERT_EQ(kDownloadSize, test_callback.response_data_length_);

    Cronet_UrlRequestParams_Destroy(request_params);
    Cronet_UrlRequest_Destroy(request);
    Cronet_UrlRequestCallback_Destroy(callback);
    Cronet_Executor_Destroy(executor);
  }
  base::Time end = base::Time::Now();
  base::TimeDelta delta = end - start;

  LOG(INFO) << "Total time " << delta.InMillisecondsF() << " ms";
  LOG(INFO) << "Single Iteration time "
            << delta.InMillisecondsF() / kTestIterations << " ms";

  double bytes_per_ms =
      kDownloadSize * kTestIterations / delta.InMillisecondsF();
  double megabytes_per_ms = bytes_per_ms / 1000000;
  double megabits_per_second = megabytes_per_ms * 8 * 1000;
  LOG(INFO) << "Average Throughput: " << megabits_per_second << " mbps";

  Cronet_EngineParams_Destroy(engine_params);
  Cronet_Engine_Destroy(engine);
  cronet::TestServer::ReleaseBigDataURL();
}

}  // namespace
