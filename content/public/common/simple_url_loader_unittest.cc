// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/simple_url_loader.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/common/network_service.mojom.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/resource_request_completion_status.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "content/public/network/network_service.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

// Server path that returns a response containing as many a's as are specified
// in the query part of the URL.
const char kResponseSizePath[] = "/response-size";

// Server path that returns a gzip response with a non-gzipped body.
const char kInvalidGzipPath[] = "/invalid-gzip";

// Server path that returns truncated response (Content-Length less than body
// size).
const char kTruncatedBodyPath[] = "/truncated-body";
// The body of the truncated response (After truncation).
const char kTruncatedBody[] = "Truncated Body";

// Class to make it easier to start a SimpleURLLoader and wait for it to
// complete.
class WaitForStringHelper {
 public:
  WaitForStringHelper() : simple_url_loader_(SimpleURLLoader::Create()) {}

  ~WaitForStringHelper() {}

  // Runs a SimpleURLLoader using the provided ResourceRequest and waits for
  // completion.
  void RunRequest(mojom::URLLoaderFactory* url_loader_factory,
                  const ResourceRequest& resource_request) {
    simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        resource_request, url_loader_factory, TRAFFIC_ANNOTATION_FOR_TESTS,
        GetCallback());
    Wait();
  }

  // Simpler version of RunRequest that takes a URL instead.
  void RunRequestForURL(mojom::URLLoaderFactory* url_loader_factory,
                        const GURL& url) {
    ResourceRequest resource_request;
    resource_request.method = "GET";
    resource_request.url = url;
    RunRequest(url_loader_factory, resource_request);
  }

  // Runs a SimpleURLLoader using the provided ResourceRequest using the
  // provided max size and waits for completion.
  void RunRequestWithBoundedSize(mojom::URLLoaderFactory* url_loader_factory,
                                 const ResourceRequest& resource_request,
                                 size_t max_size) {
    simple_url_loader_->DownloadToString(resource_request, url_loader_factory,
                                         TRAFFIC_ANNOTATION_FOR_TESTS,
                                         GetCallback(), max_size);
    Wait();
  }

  // Simpler version of RunRequestWithBoundedSize that takes a URL instead.
  void RunRequestForURLWithBoundedSize(
      mojom::URLLoaderFactory* url_loader_factory,
      const GURL& url,
      size_t max_size) {
    ResourceRequest resource_request;
    resource_request.method = "GET";
    resource_request.url = url;
    RunRequestWithBoundedSize(url_loader_factory, resource_request, max_size);
  }

  // Callback to be invoked once the SimpleURLLoader has received the response
  // body. Exposed so that some tests can start the SimpleURLLoader directly.
  SimpleURLLoader::BodyAsStringCallback GetCallback() {
    return base::BindOnce(&WaitForStringHelper::BodyReceived,
                          base::Unretained(this));
  }

  // Waits until the request is completed. Automatically called by RunRequest
  // methods, but exposed so some tests can start the SimpleURLLoader directly.
  void Wait() { run_loop_.Run(); }

  // Received response body, if any.
  const std::string* response_body() const { return response_body_.get(); }

  SimpleURLLoader* simple_url_loader() { return simple_url_loader_.get(); }

  // Returns the HTTP response code. Fails if there isn't one.
  int GetResponseCode() const {
    if (!simple_url_loader_->ResponseInfo()) {
      ADD_FAILURE() << "No response info.";
      return -1;
    }
    if (!simple_url_loader_->ResponseInfo()->headers) {
      ADD_FAILURE() << "No response headers.";
      return -1;
    }
    return simple_url_loader_->ResponseInfo()->headers->response_code();
  }

 private:
  void BodyReceived(std::unique_ptr<std::string> response_body) {
    response_body_ = std::move(response_body);
    run_loop_.Quit();
  }

  std::unique_ptr<SimpleURLLoader> simple_url_loader_;
  base::RunLoop run_loop_;

  std::unique_ptr<std::string> response_body_;

  DISALLOW_COPY_AND_ASSIGN(WaitForStringHelper);
};

// Request handler for the embedded test server that returns a response body
// with the length indicated by the query string.
std::unique_ptr<net::test_server::HttpResponse> HandleResponseSize(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path_piece() != kResponseSizePath)
    return nullptr;

  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      base::MakeUnique<net::test_server::BasicHttpResponse>();

  uint32_t length;
  if (!base::StringToUint(request.GetURL().query(), &length)) {
    ADD_FAILURE() << "Invalid length: " << request.GetURL();
  } else {
    response->set_content(std::string(length, 'a'));
  }

  return std::move(response);
}

// Request handler for the embedded test server that returns a an invalid gzip
// response body. No body bytes will be read successfully.
std::unique_ptr<net::test_server::HttpResponse> HandleInvalidGzip(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path_piece() != kInvalidGzipPath)
    return nullptr;

  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      base::MakeUnique<net::test_server::BasicHttpResponse>();
  response->AddCustomHeader("Content-Encoding", "gzip");
  response->set_content("Not gzipped");

  return std::move(response);
}

// Request handler for the embedded test server that returns a response with a
// truncated body. Consumer should see an error after reading some data.
std::unique_ptr<net::test_server::HttpResponse> HandleTruncatedBody(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path_piece() != kTruncatedBodyPath)
    return nullptr;

  std::unique_ptr<net::test_server::RawHttpResponse> response =
      base::MakeUnique<net::test_server::RawHttpResponse>(
          base::StringPrintf("HTTP/1.1 200 OK\r\n"
                             "Content-Length: %" PRIuS "\r\n",
                             strlen(kTruncatedBody) + 4),
          kTruncatedBody);

  return std::move(response);
}

class SimpleURLLoaderTest : public testing::Test {
 public:
  SimpleURLLoaderTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        network_service_(NetworkService::Create()) {
    mojom::NetworkContextParamsPtr context_params =
        mojom::NetworkContextParams::New();
    context_params->enable_data_url_support = true;
    network_service_->CreateNetworkContext(mojo::MakeRequest(&network_context_),
                                           std::move(context_params));

    network_context_->CreateURLLoaderFactory(
        mojo::MakeRequest(&url_loader_factory_), 0);

    test_server_.AddDefaultHandlers(base::FilePath(FILE_PATH_LITERAL("")));
    test_server_.RegisterRequestHandler(base::Bind(&HandleResponseSize));
    test_server_.RegisterRequestHandler(base::Bind(&HandleInvalidGzip));
    test_server_.RegisterRequestHandler(base::Bind(&HandleTruncatedBody));

    EXPECT_TRUE(test_server_.Start());
  }

  ~SimpleURLLoaderTest() override {}

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<mojom::NetworkService> network_service_;
  mojom::NetworkContextPtr network_context_;
  mojom::URLLoaderFactoryPtr url_loader_factory_;

  net::test_server::EmbeddedTestServer test_server_;
};

TEST_F(SimpleURLLoaderTest, BasicRequest) {
  ResourceRequest resource_request;
  resource_request.method = "GET";
  // Use a more interesting request than "/echo", just to verify more than the
  // request URL is hooked up.
  resource_request.url = test_server_.GetURL("/echoheader?foo");
  resource_request.headers = "foo: Expected Response";
  WaitForStringHelper string_helper;
  string_helper.RunRequest(url_loader_factory_.get(), resource_request);

  EXPECT_EQ(net::OK, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ("Expected Response", *string_helper.response_body());
}

// Test that SimpleURLLoader handles data URLs, which don't have headers.
TEST_F(SimpleURLLoaderTest, DataURL) {
  ResourceRequest resource_request;
  resource_request.method = "GET";
  // Use a more interesting request than "/echo", just to verify more than the
  // request URL is hooked up.
  resource_request.url = GURL("data:text/plain,foo");
  WaitForStringHelper string_helper;
  string_helper.RunRequest(url_loader_factory_.get(), resource_request);

  EXPECT_EQ(net::OK, string_helper.simple_url_loader()->NetError());
  ASSERT_TRUE(string_helper.simple_url_loader()->ResponseInfo());
  EXPECT_FALSE(string_helper.simple_url_loader()->ResponseInfo()->headers);
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ("foo", *string_helper.response_body());
}

// Make sure the class works when the size of the encoded and decoded bodies are
// different.
TEST_F(SimpleURLLoaderTest, GzipBody) {
  ResourceRequest resource_request;
  resource_request.method = "GET";
  resource_request.url = test_server_.GetURL("/gzip-body?foo");
  WaitForStringHelper string_helper;
  string_helper.RunRequest(url_loader_factory_.get(), resource_request);

  EXPECT_EQ(net::OK, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ("foo", *string_helper.response_body());
}

// Make sure redirects are followed.
TEST_F(SimpleURLLoaderTest, Redirect) {
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(
      url_loader_factory_.get(),
      test_server_.GetURL("/server-redirect?" +
                          test_server_.GetURL("/echo").spec()));

  EXPECT_EQ(net::OK, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ("Echo", *string_helper.response_body());
}

// Check that no body is returned with an HTTP error response.
TEST_F(SimpleURLLoaderTest, HttpErrorStatusCodeResponse) {
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(url_loader_factory_.get(),
                                 test_server_.GetURL("/echo?status=400"));

  EXPECT_EQ(net::ERR_FAILED, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(400, string_helper.GetResponseCode());
  EXPECT_FALSE(string_helper.response_body());
}

// Check that the body is returned with an HTTP error response, when
// SetAllowHttpErrorResults(true) is called.
TEST_F(SimpleURLLoaderTest, HttpErrorStatusCodeResponseAllowed) {
  WaitForStringHelper string_helper;
  string_helper.simple_url_loader()->SetAllowHttpErrorResults(true);
  string_helper.RunRequestForURL(url_loader_factory_.get(),
                                 test_server_.GetURL("/echo?status=400"));

  EXPECT_EQ(net::OK, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(400, string_helper.GetResponseCode());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ("Echo", *string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, EmptyResponseBody) {
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(url_loader_factory_.get(),
                                 test_server_.GetURL("/nocontent"));

  EXPECT_EQ(net::OK, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(204, string_helper.GetResponseCode());
  ASSERT_TRUE(string_helper.response_body());
  // A response body is sent from the NetworkService, but it's empty.
  EXPECT_EQ("", *string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, BigResponseBody) {
  WaitForStringHelper string_helper;
  // Big response that requires multiple reads, and exceeds the maximum size
  // limit of SimpleURLLoader::DownloadToString().  That is, this test make sure
  // that DownloadToStringOfUnboundedSizeUntilCrashAndDie() can receive strings
  // longer than DownloadToString() allows.
  const uint32_t kResponseSize = 2 * 1024 * 1024;
  string_helper.RunRequestForURL(url_loader_factory_.get(),
                                 test_server_.GetURL(base::StringPrintf(
                                     "/response-size?%u", kResponseSize)));

  EXPECT_EQ(net::OK, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ(kResponseSize, string_helper.response_body()->length());
  EXPECT_EQ(std::string(kResponseSize, 'a'), *string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, ResponseBodyWithSizeMatchingLimit) {
  const uint32_t kResponseSize = 16;
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kResponseSize);

  EXPECT_EQ(net::OK, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ(kResponseSize, string_helper.response_body()->length());
  EXPECT_EQ(std::string(kResponseSize, 'a'), *string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, ResponseBodyWithSizeBelowLimit) {
  WaitForStringHelper string_helper;
  const uint32_t kResponseSize = 16;
  const uint32_t kMaxResponseSize = kResponseSize + 1;
  string_helper.RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kMaxResponseSize);

  EXPECT_EQ(net::OK, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ(kResponseSize, string_helper.response_body()->length());
  EXPECT_EQ(std::string(kResponseSize, 'a'), *string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, ResponseBodyWithSizeAboveLimit) {
  const uint32_t kResponseSize = 16;
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kResponseSize - 1);

  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
            string_helper.simple_url_loader()->NetError());
  EXPECT_FALSE(string_helper.response_body());
}

// Same as above, but with setting allow_partial_results to true.
TEST_F(SimpleURLLoaderTest, ResponseBodyWithSizeAboveLimitPartialResponse) {
  WaitForStringHelper string_helper;
  const uint32_t kResponseSize = 16;
  const uint32_t kMaxResponseSize = kResponseSize - 1;
  string_helper.simple_url_loader()->SetAllowPartialResults(true);
  string_helper.RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kMaxResponseSize);

  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
            string_helper.simple_url_loader()->NetError());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ(std::string(kMaxResponseSize, 'a'), *string_helper.response_body());
  EXPECT_EQ(kMaxResponseSize, string_helper.response_body()->length());
}

// The next 4 tests duplicate the above 4, but with larger response sizes. This
// means the size limit will not be exceeded on the first read.
TEST_F(SimpleURLLoaderTest, BigResponseBodyWithSizeMatchingLimit) {
  const uint32_t kResponseSize = 512 * 1024;
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kResponseSize);

  EXPECT_EQ(net::OK, string_helper.simple_url_loader()->NetError());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ(kResponseSize, string_helper.response_body()->length());
  EXPECT_EQ(std::string(kResponseSize, 'a'), *string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, BigResponseBodyWithSizeBelowLimit) {
  const uint32_t kResponseSize = 512 * 1024;
  const uint32_t kMaxResponseSize = kResponseSize + 1;
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kMaxResponseSize);

  EXPECT_EQ(net::OK, string_helper.simple_url_loader()->NetError());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ(kResponseSize, string_helper.response_body()->length());
  EXPECT_EQ(std::string(kResponseSize, 'a'), *string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, BigResponseBodyWithSizeAboveLimit) {
  WaitForStringHelper string_helper;
  const uint32_t kResponseSize = 512 * 1024;
  string_helper.RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kResponseSize - 1);

  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
            string_helper.simple_url_loader()->NetError());
  EXPECT_FALSE(string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, BigResponseBodyWithSizeAboveLimitPartialResponse) {
  const uint32_t kResponseSize = 512 * 1024;
  const uint32_t kMaxResponseSize = kResponseSize - 1;
  WaitForStringHelper string_helper;
  string_helper.simple_url_loader()->SetAllowPartialResults(true);
  string_helper.RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kMaxResponseSize);

  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
            string_helper.simple_url_loader()->NetError());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ(std::string(kMaxResponseSize, 'a'), *string_helper.response_body());
  EXPECT_EQ(kMaxResponseSize, string_helper.response_body()->length());
}

TEST_F(SimpleURLLoaderTest, NetErrorBeforeHeaders) {
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(url_loader_factory_.get(),
                                 test_server_.GetURL("/close-socket"));

  EXPECT_EQ(net::ERR_EMPTY_RESPONSE,
            string_helper.simple_url_loader()->NetError());
  EXPECT_FALSE(string_helper.simple_url_loader()->ResponseInfo());
  EXPECT_FALSE(string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, NetErrorBeforeHeadersWithPartialResults) {
  WaitForStringHelper string_helper;
  // Allow response body on error. There should still be no response body, since
  // the error is before body reading starts.
  string_helper.simple_url_loader()->SetAllowPartialResults(true);
  string_helper.RunRequestForURL(url_loader_factory_.get(),
                                 test_server_.GetURL("/close-socket"));

  EXPECT_FALSE(string_helper.response_body());
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE,
            string_helper.simple_url_loader()->NetError());
  EXPECT_FALSE(string_helper.simple_url_loader()->ResponseInfo());
}

TEST_F(SimpleURLLoaderTest, NetErrorAfterHeaders) {
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(url_loader_factory_.get(),
                                 test_server_.GetURL(kInvalidGzipPath));

  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED,
            string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  EXPECT_FALSE(string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, NetErrorAfterHeadersWithPartialResults) {
  WaitForStringHelper string_helper;
  // Allow response body on error. This case results in a 0-byte response body.
  string_helper.simple_url_loader()->SetAllowPartialResults(true);
  string_helper.RunRequestForURL(url_loader_factory_.get(),
                                 test_server_.GetURL(kInvalidGzipPath));

  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED,
            string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ("", *string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, TruncatedBody) {
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(url_loader_factory_.get(),
                                 test_server_.GetURL(kTruncatedBodyPath));

  EXPECT_EQ(net::ERR_CONTENT_LENGTH_MISMATCH,
            string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  EXPECT_FALSE(string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, TruncatedBodyWithPartialResults) {
  WaitForStringHelper string_helper;
  string_helper.simple_url_loader()->SetAllowPartialResults(true);
  string_helper.RunRequestForURL(url_loader_factory_.get(),
                                 test_server_.GetURL(kTruncatedBodyPath));

  EXPECT_EQ(net::ERR_CONTENT_LENGTH_MISMATCH,
            string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ(kTruncatedBody, *string_helper.response_body());
}

// Test case where NetworkService is destroyed before headers are received (and
// before the request is even made, for that matter).
TEST_F(SimpleURLLoaderTest, DestroyServiceBeforeResponseStarts) {
  ResourceRequest resource_request;
  resource_request.method = "GET";
  resource_request.url = test_server_.GetURL("/hung");
  WaitForStringHelper string_helper;
  string_helper.simple_url_loader()
      ->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
          resource_request, url_loader_factory_.get(),
          TRAFFIC_ANNOTATION_FOR_TESTS, string_helper.GetCallback());
  network_service_ = nullptr;
  string_helper.Wait();

  EXPECT_EQ(net::ERR_FAILED, string_helper.simple_url_loader()->NetError());
  EXPECT_FALSE(string_helper.response_body());
  ASSERT_FALSE(string_helper.simple_url_loader()->ResponseInfo());
}

enum class TestLoaderEvent {
  kReceivedRedirect,
  kReceivedResponse,
  kBodyBufferReceived,
  kBodyDataRead,
  // ResponseComplete indicates a success.
  kResponseComplete,
  // ResponseComplete is passed a network error (net::ERR_TIMED_OUT).
  kResponseCompleteFailed,
  // Less body data is received than is expected.
  kResponseCompleteTruncated,
  // More body data is received than is expected.
  kResponseCompleteWithExtraData,
  kClientPipeClosed,
  kBodyBufferClosed,
};

// URLLoader that the test fixture can control. This allows finer grained
// control over event order over when a pipe is closed, and in ordering of
// events where there are multiple pipes. It also allows sending events in
// unexpected order, to test handling of events from less trusted processes.
class MockURLLoader : public mojom::URLLoader {
 public:
  MockURLLoader(base::test::ScopedTaskEnvironment* scoped_task_environment,
                mojom::URLLoaderRequest url_loader_request,
                mojom::URLLoaderClientPtr client,
                std::vector<TestLoaderEvent> test_events)
      : scoped_task_environment_(scoped_task_environment),
        binding_(this, std::move(url_loader_request)),
        client_(std::move(client)),
        test_events_(std::move(test_events)) {}

  void RunTest() {
    for (auto test_event : test_events_) {
      switch (test_event) {
        case TestLoaderEvent::kReceivedRedirect: {
          net::RedirectInfo redirect_info;
          redirect_info.new_method = "GET";
          redirect_info.new_url = GURL("bar://foo/");
          redirect_info.status_code = 301;

          ResourceResponseHead response_info;
          std::string headers(
              "HTTP/1.0 301 The Response Has Moved to Another Server\n"
              "Location: bar://foo/");
          response_info.headers =
              new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
                  headers.c_str(), headers.size()));
          client_->OnReceiveRedirect(redirect_info, response_info);
          break;
        }
        case TestLoaderEvent::kReceivedResponse: {
          ResourceResponseHead response_info;
          std::string headers("HTTP/1.0 200 OK");
          response_info.headers =
              new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
                  headers.c_str(), headers.size()));
          client_->OnReceiveResponse(response_info,
                                     base::Optional<net::SSLInfo>(), nullptr);
          break;
        }
        case TestLoaderEvent::kBodyBufferReceived: {
          mojo::DataPipe data_pipe(1024);
          body_stream_ = std::move(data_pipe.producer_handle);
          client_->OnStartLoadingResponseBody(
              std::move(data_pipe.consumer_handle));
          break;
        }
        case TestLoaderEvent::kBodyDataRead: {
          uint32_t num_bytes = 1;
          // Writing one byte should always succeed synchronously, for the
          // amount of data these tests send.
          EXPECT_EQ(MOJO_RESULT_OK,
                    body_stream_->WriteData("a", &num_bytes,
                                            MOJO_WRITE_DATA_FLAG_NONE));
          EXPECT_EQ(num_bytes, 1u);
          break;
        }
        case TestLoaderEvent::kResponseComplete: {
          ResourceRequestCompletionStatus request_complete_data;
          request_complete_data.error_code = net::OK;
          request_complete_data.decoded_body_length = CountBytesToSend();
          client_->OnComplete(request_complete_data);
          break;
        }
        case TestLoaderEvent::kResponseCompleteFailed: {
          ResourceRequestCompletionStatus request_complete_data;
          // Use an error that SimpleURLLoader doesn't create itself, so clear
          // when this is the source of the error code.
          request_complete_data.error_code = net::ERR_TIMED_OUT;
          request_complete_data.decoded_body_length = CountBytesToSend();
          client_->OnComplete(request_complete_data);
          break;
        }
        case TestLoaderEvent::kResponseCompleteTruncated: {
          ResourceRequestCompletionStatus request_complete_data;
          request_complete_data.error_code = net::OK;
          request_complete_data.decoded_body_length = CountBytesToSend() + 1;
          client_->OnComplete(request_complete_data);
          break;
        }
        case TestLoaderEvent::kResponseCompleteWithExtraData: {
          // Make sure |decoded_body_length| doesn't underflow.
          DCHECK_GT(CountBytesToSend(), 0u);
          ResourceRequestCompletionStatus request_complete_data;
          request_complete_data.error_code = net::OK;
          request_complete_data.decoded_body_length = CountBytesToSend() - 1;
          client_->OnComplete(request_complete_data);
        }
        case TestLoaderEvent::kClientPipeClosed: {
          EXPECT_TRUE(binding_.is_bound());
          client_.reset();
          break;
        }
        case TestLoaderEvent::kBodyBufferClosed: {
          body_stream_.reset();
          break;
        }
      }
      // Wait for Mojo to pass along the message, to ensure expected ordering.
      scoped_task_environment_->RunUntilIdle();
    }
  }
  ~MockURLLoader() override {}

  // mojom::URLLoader implementation:
  void FollowRedirect() override { NOTREACHED(); }
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    NOTREACHED();
  }

  mojom::URLLoaderClient* client() const { return client_.get(); }

 private:
  // Counts the total number of bytes that will be sent over the course of
  // running the request. Includes both those that have been sent already, and
  // those that have yet to be sent.
  uint32_t CountBytesToSend() const {
    int total_bytes = 0;
    for (auto test_event : test_events_) {
      if (test_event == TestLoaderEvent::kBodyDataRead)
        ++total_bytes;
    }
    return total_bytes;
  }

  base::test::ScopedTaskEnvironment* scoped_task_environment_;

  std::unique_ptr<net::URLRequest> url_request_;
  mojo::Binding<mojom::URLLoader> binding_;
  mojom::URLLoaderClientPtr client_;

  std::vector<TestLoaderEvent> test_events_;

  mojo::ScopedDataPipeProducerHandle body_stream_;

  DISALLOW_COPY_AND_ASSIGN(MockURLLoader);
};

class MockURLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  explicit MockURLLoaderFactory(
      base::test::ScopedTaskEnvironment* scoped_task_environment)
      : scoped_task_environment_(scoped_task_environment) {}
  ~MockURLLoaderFactory() override {}

  // mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(mojom::URLLoaderRequest url_loader_request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& url_request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    EXPECT_FALSE(url_loader_);
    url_loader_ = base::MakeUnique<MockURLLoader>(
        scoped_task_environment_, std::move(url_loader_request),
        std::move(client), std::move(test_events_));
    url_loader_->RunTest();
  }
  void Clone(mojom::URLLoaderFactoryRequest request) override { NOTREACHED(); }

  MockURLLoader* url_loader() const { return url_loader_.get(); }

  void AddEvent(TestLoaderEvent test_event) {
    DCHECK(!url_loader_);
    test_events_.push_back(test_event);
  }

 private:
  base::test::ScopedTaskEnvironment* scoped_task_environment_;
  std::unique_ptr<MockURLLoader> url_loader_;
  std::vector<TestLoaderEvent> test_events_;

  DISALLOW_COPY_AND_ASSIGN(MockURLLoaderFactory);
};

// Check that the request fails if OnComplete() is called before anything else.
TEST_F(SimpleURLLoaderTest, ResponseCompleteBeforeReceivedResponse) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvent(TestLoaderEvent::kResponseComplete);
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED, string_helper.simple_url_loader()->NetError());
  EXPECT_FALSE(string_helper.simple_url_loader()->ResponseInfo());
  EXPECT_FALSE(string_helper.response_body());
}

// Check that the request fails if OnComplete() is called before the body pipe
// is received.
TEST_F(SimpleURLLoaderTest, ResponseCompleteAfterReceivedResponse) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedResponse);
  loader_factory.AddEvent(TestLoaderEvent::kResponseComplete);
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  EXPECT_FALSE(string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, CloseClientPipeBeforeBodyStarts) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedResponse);
  loader_factory.AddEvent(TestLoaderEvent::kClientPipeClosed);
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_FAILED, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  EXPECT_FALSE(string_helper.response_body());
}

// This test tries closing the client pipe / completing the request in most
// possible valid orders relative to read events (Which always occur in the same
// order).
TEST_F(SimpleURLLoaderTest, CloseClientPipeOrder) {
  enum class ClientCloseOrder {
    kBeforeData,
    kDuringData,
    kAfterData,
    kAfterBufferClosed,
  };

  // In what order the URLLoaderClient pipe is closed, relative to read events.
  // Order of other main events can't vary, relative to each other (Getting body
  // pipe, reading body bytes, closing body pipe).
  const ClientCloseOrder kClientCloseOrder[] = {
      ClientCloseOrder::kBeforeData, ClientCloseOrder::kDuringData,
      ClientCloseOrder::kAfterData, ClientCloseOrder::kAfterBufferClosed,
  };

  const TestLoaderEvent kClientCloseEvents[] = {
      TestLoaderEvent::kResponseComplete,
      TestLoaderEvent::kResponseCompleteFailed,
      TestLoaderEvent::kResponseCompleteTruncated,
      TestLoaderEvent::kClientPipeClosed,
  };

  for (const auto close_client_order : kClientCloseOrder) {
    for (const auto close_client_event : kClientCloseEvents) {
      for (uint32_t bytes_received = 0; bytes_received < 3; bytes_received++) {
        for (int allow_partial_results = 0; allow_partial_results < 2;
             allow_partial_results++) {
          if (close_client_order == ClientCloseOrder::kDuringData &&
              bytes_received < 2) {
            continue;
          }
          MockURLLoaderFactory loader_factory(&scoped_task_environment_);
          loader_factory.AddEvent(TestLoaderEvent::kReceivedResponse);
          loader_factory.AddEvent(TestLoaderEvent::kBodyBufferReceived);
          if (close_client_order == ClientCloseOrder::kBeforeData)
            loader_factory.AddEvent(close_client_event);

          for (uint32_t i = 0; i < bytes_received; ++i) {
            loader_factory.AddEvent(TestLoaderEvent::kBodyDataRead);
            if (i == 0 && close_client_order == ClientCloseOrder::kDuringData)
              loader_factory.AddEvent(close_client_event);
          }

          if (close_client_order == ClientCloseOrder::kAfterData)
            loader_factory.AddEvent(close_client_event);
          loader_factory.AddEvent(TestLoaderEvent::kBodyBufferClosed);
          if (close_client_order == ClientCloseOrder::kAfterBufferClosed)
            loader_factory.AddEvent(close_client_event);

          WaitForStringHelper string_helper;
          string_helper.simple_url_loader()->SetAllowPartialResults(
              allow_partial_results);
          string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

          EXPECT_EQ(200, string_helper.GetResponseCode());
          if (close_client_event != TestLoaderEvent::kResponseComplete) {
            if (close_client_event ==
                TestLoaderEvent::kResponseCompleteFailed) {
              EXPECT_EQ(net::ERR_TIMED_OUT,
                        string_helper.simple_url_loader()->NetError());
            } else {
              EXPECT_EQ(net::ERR_FAILED,
                        string_helper.simple_url_loader()->NetError());
            }
            if (!allow_partial_results) {
              EXPECT_FALSE(string_helper.response_body());
            } else {
              ASSERT_TRUE(string_helper.response_body());
              EXPECT_EQ(std::string(bytes_received, 'a'),
                        *string_helper.response_body());
            }
          } else {
            EXPECT_EQ(net::OK, string_helper.simple_url_loader()->NetError());
            ASSERT_TRUE(string_helper.response_body());
            EXPECT_EQ(std::string(bytes_received, 'a'),
                      *string_helper.response_body());
          }
        }
      }
    }
  }
}

// Make sure the close client pipe message doesn't cause any issues.
TEST_F(SimpleURLLoaderTest, ErrorAndCloseClientPipeBeforeBodyStarts) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedResponse);
  loader_factory.AddEvent(TestLoaderEvent::kResponseCompleteFailed);
  loader_factory.AddEvent(TestLoaderEvent::kClientPipeClosed);
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_TIMED_OUT, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  EXPECT_FALSE(string_helper.response_body());
}

// Make sure the close client pipe message doesn't cause any issues.
TEST_F(SimpleURLLoaderTest, SuccessAndCloseClientPipeBeforeBodyComplete) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedResponse);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferReceived);
  loader_factory.AddEvent(TestLoaderEvent::kResponseComplete);
  loader_factory.AddEvent(TestLoaderEvent::kClientPipeClosed);
  loader_factory.AddEvent(TestLoaderEvent::kBodyDataRead);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferClosed);
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::OK, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ("a", *string_helper.response_body());
}

// Make sure the close client pipe message doesn't cause any issues.
TEST_F(SimpleURLLoaderTest, SuccessAndCloseClientPipeAfterBodyComplete) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedResponse);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferReceived);
  loader_factory.AddEvent(TestLoaderEvent::kBodyDataRead);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferClosed);
  loader_factory.AddEvent(TestLoaderEvent::kResponseComplete);
  loader_factory.AddEvent(TestLoaderEvent::kClientPipeClosed);
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::OK, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  ASSERT_TRUE(string_helper.response_body());
  EXPECT_EQ("a", *string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, DoubleReceivedResponse) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedResponse);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedResponse);
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  EXPECT_FALSE(string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, RedirectAfterReseivedResponse) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedResponse);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedRedirect);
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  EXPECT_FALSE(string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, UnexpectedBodyBufferReceived) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferReceived);
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED, string_helper.simple_url_loader()->NetError());
  EXPECT_FALSE(string_helper.simple_url_loader()->ResponseInfo());
  EXPECT_FALSE(string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, DoubleBodyBufferReceived) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedResponse);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferReceived);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferReceived);
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  EXPECT_FALSE(string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, UnexpectedMessageAfterBodyStarts) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedResponse);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferReceived);
  loader_factory.AddEvent(TestLoaderEvent::kBodyDataRead);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedRedirect);
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  EXPECT_FALSE(string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, UnexpectedMessageAfterBodyStarts2) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedResponse);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferReceived);
  loader_factory.AddEvent(TestLoaderEvent::kBodyDataRead);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferReceived);
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  EXPECT_FALSE(string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, UnexpectedMessageAfterBodyComplete) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedResponse);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferReceived);
  loader_factory.AddEvent(TestLoaderEvent::kBodyDataRead);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferClosed);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferReceived);
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  EXPECT_FALSE(string_helper.response_body());
}

TEST_F(SimpleURLLoaderTest, MoreDataThanExpected) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvent(TestLoaderEvent::kReceivedResponse);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferReceived);
  loader_factory.AddEvent(TestLoaderEvent::kBodyDataRead);
  loader_factory.AddEvent(TestLoaderEvent::kBodyDataRead);
  loader_factory.AddEvent(TestLoaderEvent::kBodyBufferClosed);
  loader_factory.AddEvent(TestLoaderEvent::kResponseCompleteWithExtraData);
  WaitForStringHelper string_helper;
  string_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED, string_helper.simple_url_loader()->NetError());
  EXPECT_EQ(200, string_helper.GetResponseCode());
  EXPECT_FALSE(string_helper.response_body());
}

}  // namespace
}  // namespace content
