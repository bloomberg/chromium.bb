// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/simple_url_loader.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
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
#include "mojo/public/cpp/bindings/binding_set.h"
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

// Class to make it easier to start a SimpleURLLoader, wait for it to complete,
// and check the result.
class SimpleLoaderTestHelper {
 public:
  // What the response should be downloaded to.
  enum class DownloadType { TO_STRING, TO_FILE };

  explicit SimpleLoaderTestHelper(DownloadType download_type)
      : download_type_(download_type),
        simple_url_loader_(SimpleURLLoader::Create()) {
    // Create a desistination directory, if downloading to a file.
    if (download_type_ == DownloadType::TO_FILE) {
      EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
      dest_path_ = temp_dir_.GetPath().AppendASCII("foo");
    }
  }

  ~SimpleLoaderTestHelper() {}

  // File path that will be written to.
  const base::FilePath& dest_path() const {
    DCHECK_EQ(DownloadType::TO_FILE, download_type_);
    return dest_path_;
  }

  // Starts a SimpleURLLoader using the method corresponding to the
  // DownloadType, but does not wait for it to complete. The default
  // |max_body_size| of -1 means don't use a max body size (Use
  // DownloadToStringOfUnboundedSizeUntilCrashAndDie for string downloads, and
  // don't specify a size for other types of downloads).
  void StartRequest(mojom::URLLoaderFactory* url_loader_factory,
                    const ResourceRequest& resource_request,
                    int64_t max_body_size = -1) {
    EXPECT_FALSE(done_);
    switch (download_type_) {
      case DownloadType::TO_STRING:
        if (max_body_size < 0) {
          simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
              resource_request, url_loader_factory,
              TRAFFIC_ANNOTATION_FOR_TESTS,
              base::BindOnce(&SimpleLoaderTestHelper::DownloadedToString,
                             base::Unretained(this)));
        } else {
          simple_url_loader_->DownloadToString(
              resource_request, url_loader_factory,
              TRAFFIC_ANNOTATION_FOR_TESTS,
              base::BindOnce(&SimpleLoaderTestHelper::DownloadedToString,
                             base::Unretained(this)),
              max_body_size);
        }
        break;
      case DownloadType::TO_FILE:
        if (max_body_size < 0) {
          simple_url_loader_->DownloadToFile(
              resource_request, url_loader_factory,
              TRAFFIC_ANNOTATION_FOR_TESTS,
              base::BindOnce(&SimpleLoaderTestHelper::DownloadedToFile,
                             base::Unretained(this)),
              dest_path_);
        } else {
          simple_url_loader_->DownloadToFile(
              resource_request, url_loader_factory,
              TRAFFIC_ANNOTATION_FOR_TESTS,
              base::BindOnce(&SimpleLoaderTestHelper::DownloadedToFile,
                             base::Unretained(this)),
              dest_path_, max_body_size);
        }
        break;
    }
  }

  // Runs a SimpleURLLoader using the provided ResourceRequest and waits for
  // completion.
  void RunRequest(mojom::URLLoaderFactory* url_loader_factory,
                  const ResourceRequest& resource_request,
                  int64_t max_body_size = -1) {
    StartRequest(url_loader_factory, resource_request, max_body_size);
    Wait();
  }

  // Simpler version of RunRequest that takes a URL instead.
  void RunRequestForURL(mojom::URLLoaderFactory* url_loader_factory,
                        const GURL& url) {
    ResourceRequest resource_request;
    resource_request.url = url;
    RunRequest(url_loader_factory, resource_request);
  }

  // Runs a SimpleURLLoader using the provided ResourceRequest using the
  // provided max size and waits for completion.
  void RunRequestWithBoundedSize(mojom::URLLoaderFactory* url_loader_factory,
                                 const ResourceRequest& resource_request,
                                 int64_t max_body_size) {
    RunRequest(url_loader_factory, resource_request, max_body_size);
  }

  // Simpler version of RunRequestWithBoundedSize that takes a URL instead.
  void RunRequestForURLWithBoundedSize(
      mojom::URLLoaderFactory* url_loader_factory,
      const GURL& url,
      size_t max_size) {
    ResourceRequest resource_request;
    resource_request.url = url;
    RunRequestWithBoundedSize(url_loader_factory, resource_request, max_size);
  }

  // Waits until the request is completed. Automatically called by RunRequest
  // methods, but exposed so some tests can start the SimpleURLLoader directly.
  void Wait() { run_loop_.Run(); }

  // Sets whether a file should still exists on download-to-file errors.
  // Defaults to false.
  void set_expect_path_exists_on_error(bool expect_path_exists_on_error) {
    EXPECT_EQ(DownloadType::TO_FILE, download_type_);
    expect_path_exists_on_error_ = expect_path_exists_on_error;
  }

  // Received response body, if any. Returns nullptr if no body was received
  // (Which is different from a 0-length body). For DownloadType::TO_STRING,
  // this is just the value passed to the callback. For DownloadType::TO_FILE,
  // it is nullptr if an empty FilePath was passed to the callback, or the
  // contents of the file, otherwise.
  const std::string* response_body() const {
    EXPECT_TRUE(done_);
    return response_body_.get();
  }

  // Returns true if the callback has been invoked.
  bool done() const { return done_; }

  SimpleURLLoader* simple_url_loader() { return simple_url_loader_.get(); }

  // Returns the HTTP response code. Fails if there isn't one.
  int GetResponseCode() const {
    EXPECT_TRUE(done_);
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
  void DownloadedToString(std::unique_ptr<std::string> response_body) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    EXPECT_FALSE(done_);
    EXPECT_EQ(DownloadType::TO_STRING, download_type_);
    EXPECT_FALSE(response_body_);

    response_body_ = std::move(response_body);

    done_ = true;
    run_loop_.Quit();
  }

  void DownloadedToFile(const base::FilePath& file_path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    EXPECT_FALSE(done_);
    EXPECT_EQ(DownloadType::TO_FILE, download_type_);
    EXPECT_FALSE(response_body_);

    // Make sure the destination file exists if |file_path| is non-empty, or the
    // file is expected to exist on error.
    EXPECT_EQ(!file_path.empty() || expect_path_exists_on_error_,
              base::PathExists(dest_path_));

    if (!file_path.empty()) {
      EXPECT_EQ(dest_path_, file_path);
      response_body_ = std::make_unique<std::string>();
      EXPECT_TRUE(base::ReadFileToString(dest_path_, response_body_.get()));
    }

    done_ = true;
    run_loop_.Quit();
  }

  DownloadType download_type_;
  bool done_ = false;

  bool expect_path_exists_on_error_ = false;

  std::unique_ptr<SimpleURLLoader> simple_url_loader_;
  base::RunLoop run_loop_;

  std::unique_ptr<std::string> response_body_;

  base::ScopedTempDir temp_dir_;
  base::FilePath dest_path_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SimpleLoaderTestHelper);
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

// Base class with shared setup logic.
class SimpleURLLoaderTestBase {
 public:
  SimpleURLLoaderTestBase()
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

  virtual ~SimpleURLLoaderTestBase() {}

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<mojom::NetworkService> network_service_;
  mojom::NetworkContextPtr network_context_;
  mojom::URLLoaderFactoryPtr url_loader_factory_;

  net::test_server::EmbeddedTestServer test_server_;
};

class SimpleURLLoaderTest
    : public SimpleURLLoaderTestBase,
      public testing::TestWithParam<SimpleLoaderTestHelper::DownloadType> {
 public:
  SimpleURLLoaderTest() : test_helper_(GetParam()) {}

  ~SimpleURLLoaderTest() override {}

  SimpleLoaderTestHelper* test_helper() { return &test_helper_; }

 private:
  SimpleLoaderTestHelper test_helper_;
};

TEST_P(SimpleURLLoaderTest, BasicRequest) {
  ResourceRequest resource_request;
  // Use a more interesting request than "/echo", just to verify more than the
  // request URL is hooked up.
  resource_request.url = test_server_.GetURL("/echoheader?foo");
  resource_request.headers.SetHeader("foo", "Expected Response");
  test_helper()->RunRequest(url_loader_factory_.get(), resource_request);

  EXPECT_EQ(net::OK, test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ("Expected Response", *test_helper()->response_body());
}

// Test that SimpleURLLoader handles data URLs, which don't have headers.
TEST_P(SimpleURLLoaderTest, DataURL) {
  ResourceRequest resource_request;
  resource_request.url = GURL("data:text/plain,foo");
  test_helper()->RunRequest(url_loader_factory_.get(), resource_request);

  EXPECT_EQ(net::OK, test_helper()->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper()->simple_url_loader()->ResponseInfo());
  EXPECT_FALSE(test_helper()->simple_url_loader()->ResponseInfo()->headers);
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ("foo", *test_helper()->response_body());
}

// Make sure the class works when the size of the encoded and decoded bodies are
// different.
TEST_P(SimpleURLLoaderTest, GzipBody) {
  ResourceRequest resource_request;
  resource_request.url = test_server_.GetURL("/gzip-body?foo");
  test_helper()->RunRequest(url_loader_factory_.get(), resource_request);

  EXPECT_EQ(net::OK, test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ("foo", *test_helper()->response_body());
}

// Make sure redirects are followed.
TEST_P(SimpleURLLoaderTest, Redirect) {
  test_helper()->RunRequestForURL(
      url_loader_factory_.get(),
      test_server_.GetURL("/server-redirect?" +
                          test_server_.GetURL("/echo").spec()));

  EXPECT_EQ(net::OK, test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ("Echo", *test_helper()->response_body());
}

// Check that no body is returned with an HTTP error response.
TEST_P(SimpleURLLoaderTest, HttpErrorStatusCodeResponse) {
  test_helper()->RunRequestForURL(url_loader_factory_.get(),
                                  test_server_.GetURL("/echo?status=400"));

  EXPECT_EQ(net::ERR_FAILED, test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(400, test_helper()->GetResponseCode());
  EXPECT_FALSE(test_helper()->response_body());
}

// Check that the body is returned with an HTTP error response, when
// SetAllowHttpErrorResults(true) is called.
TEST_P(SimpleURLLoaderTest, HttpErrorStatusCodeResponseAllowed) {
  test_helper()->simple_url_loader()->SetAllowHttpErrorResults(true);
  test_helper()->RunRequestForURL(url_loader_factory_.get(),
                                  test_server_.GetURL("/echo?status=400"));

  EXPECT_EQ(net::OK, test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(400, test_helper()->GetResponseCode());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ("Echo", *test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, EmptyResponseBody) {
  test_helper()->RunRequestForURL(url_loader_factory_.get(),
                                  test_server_.GetURL("/nocontent"));

  EXPECT_EQ(net::OK, test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(204, test_helper()->GetResponseCode());
  ASSERT_TRUE(test_helper()->response_body());
  // A response body is sent from the NetworkService, but it's empty.
  EXPECT_EQ("", *test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, BigResponseBody) {
  // Big response that requires multiple reads, and exceeds the maximum size
  // limit of SimpleURLLoader::DownloadToString().  That is, this test make sure
  // that DownloadToStringOfUnboundedSizeUntilCrashAndDie() can receive strings
  // longer than DownloadToString() allows.
  const uint32_t kResponseSize = 2 * 1024 * 1024;
  test_helper()->RunRequestForURL(url_loader_factory_.get(),
                                  test_server_.GetURL(base::StringPrintf(
                                      "/response-size?%u", kResponseSize)));

  EXPECT_EQ(net::OK, test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ(kResponseSize, test_helper()->response_body()->length());
  EXPECT_EQ(std::string(kResponseSize, 'a'), *test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, ResponseBodyWithSizeMatchingLimit) {
  const uint32_t kResponseSize = 16;
  test_helper()->RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kResponseSize);

  EXPECT_EQ(net::OK, test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ(kResponseSize, test_helper()->response_body()->length());
  EXPECT_EQ(std::string(kResponseSize, 'a'), *test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, ResponseBodyWithSizeBelowLimit) {
  const uint32_t kResponseSize = 16;
  const uint32_t kMaxResponseSize = kResponseSize + 1;
  test_helper()->RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kMaxResponseSize);

  EXPECT_EQ(net::OK, test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ(kResponseSize, test_helper()->response_body()->length());
  EXPECT_EQ(std::string(kResponseSize, 'a'), *test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, ResponseBodyWithSizeAboveLimit) {
  const uint32_t kResponseSize = 16;
  test_helper()->RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kResponseSize - 1);

  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper()->response_body());
}

// Same as above, but with setting allow_partial_results to true.
TEST_P(SimpleURLLoaderTest, ResponseBodyWithSizeAboveLimitPartialResponse) {
  const uint32_t kResponseSize = 16;
  const uint32_t kMaxResponseSize = kResponseSize - 1;
  test_helper()->simple_url_loader()->SetAllowPartialResults(true);
  test_helper()->RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kMaxResponseSize);

  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
            test_helper()->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ(std::string(kMaxResponseSize, 'a'),
            *test_helper()->response_body());
  EXPECT_EQ(kMaxResponseSize, test_helper()->response_body()->length());
}

// The next 4 tests duplicate the above 4, but with larger response sizes. This
// means the size limit will not be exceeded on the first read.
TEST_P(SimpleURLLoaderTest, BigResponseBodyWithSizeMatchingLimit) {
  const uint32_t kResponseSize = 512 * 1024;
  test_helper()->RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kResponseSize);

  EXPECT_EQ(net::OK, test_helper()->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ(kResponseSize, test_helper()->response_body()->length());
  EXPECT_EQ(std::string(kResponseSize, 'a'), *test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, BigResponseBodyWithSizeBelowLimit) {
  const uint32_t kResponseSize = 512 * 1024;
  const uint32_t kMaxResponseSize = kResponseSize + 1;
  test_helper()->RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kMaxResponseSize);

  EXPECT_EQ(net::OK, test_helper()->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ(kResponseSize, test_helper()->response_body()->length());
  EXPECT_EQ(std::string(kResponseSize, 'a'), *test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, BigResponseBodyWithSizeAboveLimit) {
  const uint32_t kResponseSize = 512 * 1024;
  test_helper()->RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kResponseSize - 1);

  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, BigResponseBodyWithSizeAboveLimitPartialResponse) {
  const uint32_t kResponseSize = 512 * 1024;
  const uint32_t kMaxResponseSize = kResponseSize - 1;
  test_helper()->simple_url_loader()->SetAllowPartialResults(true);
  test_helper()->RunRequestForURLWithBoundedSize(
      url_loader_factory_.get(),
      test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)),
      kMaxResponseSize);

  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
            test_helper()->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ(std::string(kMaxResponseSize, 'a'),
            *test_helper()->response_body());
  EXPECT_EQ(kMaxResponseSize, test_helper()->response_body()->length());
}

TEST_P(SimpleURLLoaderTest, NetErrorBeforeHeaders) {
  test_helper()->RunRequestForURL(url_loader_factory_.get(),
                                  test_server_.GetURL("/close-socket"));

  EXPECT_EQ(net::ERR_EMPTY_RESPONSE,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper()->simple_url_loader()->ResponseInfo());
  EXPECT_FALSE(test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, NetErrorBeforeHeadersWithPartialResults) {
  // Allow response body on error. There should still be no response body, since
  // the error is before body reading starts.
  test_helper()->simple_url_loader()->SetAllowPartialResults(true);
  test_helper()->RunRequestForURL(url_loader_factory_.get(),
                                  test_server_.GetURL("/close-socket"));

  EXPECT_FALSE(test_helper()->response_body());
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper()->simple_url_loader()->ResponseInfo());
}

TEST_P(SimpleURLLoaderTest, NetErrorAfterHeaders) {
  test_helper()->RunRequestForURL(url_loader_factory_.get(),
                                  test_server_.GetURL(kInvalidGzipPath));

  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  EXPECT_FALSE(test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, NetErrorAfterHeadersWithPartialResults) {
  // Allow response body on error. This case results in a 0-byte response body.
  test_helper()->simple_url_loader()->SetAllowPartialResults(true);
  test_helper()->RunRequestForURL(url_loader_factory_.get(),
                                  test_server_.GetURL(kInvalidGzipPath));

  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ("", *test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, TruncatedBody) {
  test_helper()->RunRequestForURL(url_loader_factory_.get(),
                                  test_server_.GetURL(kTruncatedBodyPath));

  EXPECT_EQ(net::ERR_CONTENT_LENGTH_MISMATCH,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  EXPECT_FALSE(test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, TruncatedBodyWithPartialResults) {
  test_helper()->simple_url_loader()->SetAllowPartialResults(true);
  test_helper()->RunRequestForURL(url_loader_factory_.get(),
                                  test_server_.GetURL(kTruncatedBodyPath));

  EXPECT_EQ(net::ERR_CONTENT_LENGTH_MISMATCH,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ(kTruncatedBody, *test_helper()->response_body());
}

// Test case where NetworkService is destroyed before headers are received (and
// before the request is even made, for that matter).
TEST_P(SimpleURLLoaderTest, DestroyServiceBeforeResponseStarts) {
  ResourceRequest resource_request;
  resource_request.url = test_server_.GetURL("/hung");
  test_helper()->StartRequest(url_loader_factory_.get(), resource_request);
  network_service_ = nullptr;
  test_helper()->Wait();

  EXPECT_EQ(net::ERR_FAILED, test_helper()->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper()->response_body());
  ASSERT_FALSE(test_helper()->simple_url_loader()->ResponseInfo());
}

enum class TestLoaderEvent {
  kReceivedRedirect,
  // Receive a response with a 200 status code.
  kReceivedResponse,
  // Receive a response with a 401 status code.
  kReceived401Response,
  // Receive a response with a 501 status code.
  kReceived501Response,
  kBodyBufferReceived,
  kBodyDataRead,
  // ResponseComplete indicates a success.
  kResponseComplete,
  // ResponseComplete is passed a network error (net::ERR_TIMED_OUT).
  kResponseCompleteFailed,
  // ResponseComplete is passed net::ERR_NETWORK_CHANGED.
  kResponseCompleteNetworkChanged,
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
        case TestLoaderEvent::kReceived401Response: {
          ResourceResponseHead response_info;
          std::string headers("HTTP/1.0 401 Client Borkage");
          response_info.headers =
              new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
                  headers.c_str(), headers.size()));
          client_->OnReceiveResponse(response_info,
                                     base::Optional<net::SSLInfo>(), nullptr);
          break;
        }
        case TestLoaderEvent::kReceived501Response: {
          ResourceResponseHead response_info;
          std::string headers("HTTP/1.0 501 Server Borkage");
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
        case TestLoaderEvent::kResponseCompleteNetworkChanged: {
          ResourceRequestCompletionStatus request_complete_data;
          request_complete_data.error_code = net::ERR_NETWORK_CHANGED;
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
  void FollowRedirect() override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    NOTREACHED();
  }
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

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
    ASSERT_FALSE(test_events_.empty());
    requested_urls_.push_back(url_request.url);
    url_loaders_.push_back(base::MakeUnique<MockURLLoader>(
        scoped_task_environment_, std::move(url_loader_request),
        std::move(client), test_events_.front()));
    test_events_.pop_front();

    url_loader_queue_.push_back(url_loaders_.back().get());

    // To avoid nested RunTest() calls, which would result in recursive
    // RunUntilIdle() calls, only have the outermost CreateLoaderAndStart call
    // run the tasks for a URLLoader.
    if (url_loader_queue_.size() == 1) {
      while (!url_loader_queue_.empty()) {
        url_loader_queue_.front()->RunTest();
        url_loader_queue_.pop_front();
      }
    }
  }

  void Clone(mojom::URLLoaderFactoryRequest request) override {
    binding_set_.AddBinding(this, std::move(request));
  }

  // Adds a events that will be returned by a single MockURLLoader. Mutliple
  // calls mean multiple MockURLLoaders are expected to be created. Each will
  // run to completion before the next one is expected to be created.
  void AddEvents(const std::vector<TestLoaderEvent> events) {
    DCHECK(url_loaders_.empty());
    test_events_.push_back(events);
  }

  const std::list<GURL>& requested_urls() const { return requested_urls_; }

 private:
  base::test::ScopedTaskEnvironment* scoped_task_environment_;
  std::list<std::unique_ptr<MockURLLoader>> url_loaders_;
  std::list<std::vector<TestLoaderEvent>> test_events_;

  // Queue of URLLoaders that have yet to had their RunTest method called.
  // Separate list than |url_loaders_| so that old pipes aren't destroyed.
  std::list<MockURLLoader*> url_loader_queue_;

  std::list<GURL> requested_urls_;

  mojo::BindingSet<mojom::URLLoaderFactory> binding_set_;

  DISALLOW_COPY_AND_ASSIGN(MockURLLoaderFactory);
};

// Check that the request fails if OnComplete() is called before anything else.
TEST_P(SimpleURLLoaderTest, ResponseCompleteBeforeReceivedResponse) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents({TestLoaderEvent::kResponseComplete});
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper()->simple_url_loader()->ResponseInfo());
  EXPECT_FALSE(test_helper()->response_body());
}

// Check that the request fails if OnComplete() is called before the body pipe
// is received.
TEST_P(SimpleURLLoaderTest, ResponseCompleteAfterReceivedResponse) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kResponseComplete});
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  EXPECT_FALSE(test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, CloseClientPipeBeforeBodyStarts) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kClientPipeClosed});
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_FAILED, test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  EXPECT_FALSE(test_helper()->response_body());
}

// This test tries closing the client pipe / completing the request in most
// possible valid orders relative to read events (Which always occur in the same
// order).
TEST_P(SimpleURLLoaderTest, CloseClientPipeOrder) {
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
          std::vector<TestLoaderEvent> events;
          events.push_back(TestLoaderEvent::kReceivedResponse);
          events.push_back(TestLoaderEvent::kBodyBufferReceived);
          if (close_client_order == ClientCloseOrder::kBeforeData)
            events.push_back(close_client_event);

          for (uint32_t i = 0; i < bytes_received; ++i) {
            events.push_back(TestLoaderEvent::kBodyDataRead);
            if (i == 0 && close_client_order == ClientCloseOrder::kDuringData)
              events.push_back(close_client_event);
          }

          if (close_client_order == ClientCloseOrder::kAfterData)
            events.push_back(close_client_event);
          events.push_back(TestLoaderEvent::kBodyBufferClosed);
          if (close_client_order == ClientCloseOrder::kAfterBufferClosed)
            events.push_back(close_client_event);
          loader_factory.AddEvents(events);

          SimpleLoaderTestHelper test_helper(GetParam());
          test_helper.simple_url_loader()->SetAllowPartialResults(
              allow_partial_results);
          test_helper.RunRequestForURL(&loader_factory, GURL("foo://bar/"));

          EXPECT_EQ(200, test_helper.GetResponseCode());
          if (close_client_event != TestLoaderEvent::kResponseComplete) {
            if (close_client_event ==
                TestLoaderEvent::kResponseCompleteFailed) {
              EXPECT_EQ(net::ERR_TIMED_OUT,
                        test_helper.simple_url_loader()->NetError());
            } else {
              EXPECT_EQ(net::ERR_FAILED,
                        test_helper.simple_url_loader()->NetError());
            }
            if (!allow_partial_results) {
              EXPECT_FALSE(test_helper.response_body());
            } else {
              ASSERT_TRUE(test_helper.response_body());
              EXPECT_EQ(std::string(bytes_received, 'a'),
                        *test_helper.response_body());
            }
          } else {
            EXPECT_EQ(net::OK, test_helper.simple_url_loader()->NetError());
            ASSERT_TRUE(test_helper.response_body());
            EXPECT_EQ(std::string(bytes_received, 'a'),
                      *test_helper.response_body());
          }
        }
      }
    }
  }
}

// Make sure the close client pipe message doesn't cause any issues.
TEST_P(SimpleURLLoaderTest, ErrorAndCloseClientPipeBeforeBodyStarts) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents({TestLoaderEvent::kReceivedResponse,
                            TestLoaderEvent::kResponseCompleteFailed,
                            TestLoaderEvent::kClientPipeClosed});
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_TIMED_OUT, test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  EXPECT_FALSE(test_helper()->response_body());
}

// Make sure the close client pipe message doesn't cause any issues.
TEST_P(SimpleURLLoaderTest, SuccessAndCloseClientPipeBeforeBodyComplete) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyBufferReceived,
       TestLoaderEvent::kResponseComplete, TestLoaderEvent::kClientPipeClosed,
       TestLoaderEvent::kBodyDataRead, TestLoaderEvent::kBodyBufferClosed});
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::OK, test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ("a", *test_helper()->response_body());
}

// Make sure the close client pipe message doesn't cause any issues.
TEST_P(SimpleURLLoaderTest, SuccessAndCloseClientPipeAfterBodyComplete) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyBufferReceived,
       TestLoaderEvent::kBodyDataRead, TestLoaderEvent::kBodyBufferClosed,
       TestLoaderEvent::kResponseComplete, TestLoaderEvent::kClientPipeClosed});
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::OK, test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ("a", *test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, DoubleReceivedResponse) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kReceivedResponse});
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  EXPECT_FALSE(test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, RedirectAfterReseivedResponse) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kReceivedRedirect});
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  EXPECT_FALSE(test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, UnexpectedBodyBufferReceived) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents({TestLoaderEvent::kBodyBufferReceived});
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper()->simple_url_loader()->ResponseInfo());
  EXPECT_FALSE(test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, DoubleBodyBufferReceived) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents({TestLoaderEvent::kReceivedResponse,
                            TestLoaderEvent::kBodyBufferReceived,
                            TestLoaderEvent::kBodyBufferReceived});
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  EXPECT_FALSE(test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, UnexpectedMessageAfterBodyStarts) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyBufferReceived,
       TestLoaderEvent::kBodyDataRead, TestLoaderEvent::kReceivedRedirect});
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  EXPECT_FALSE(test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, UnexpectedMessageAfterBodyStarts2) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyBufferReceived,
       TestLoaderEvent::kBodyDataRead, TestLoaderEvent::kBodyBufferReceived});
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  EXPECT_FALSE(test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, UnexpectedMessageAfterBodyComplete) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyBufferReceived,
       TestLoaderEvent::kBodyDataRead, TestLoaderEvent::kBodyBufferClosed,
       TestLoaderEvent::kBodyBufferReceived});
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  EXPECT_FALSE(test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, MoreDataThanExpected) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyBufferReceived,
       TestLoaderEvent::kBodyDataRead, TestLoaderEvent::kBodyDataRead,
       TestLoaderEvent::kBodyBufferClosed,
       TestLoaderEvent::kResponseCompleteWithExtraData});
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(net::ERR_UNEXPECTED,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_EQ(200, test_helper()->GetResponseCode());
  EXPECT_FALSE(test_helper()->response_body());
}

TEST_P(SimpleURLLoaderTest, RetryOn5xx) {
  const GURL kInitialURL("foo://bar/initial");
  struct TestCase {
    // Parameters passed to SetRetryOptions.
    int max_retries;
    int retry_mode;

    // Number of 5xx responses before a successful response.
    int num_5xx;

    // Whether the request is expected to succeed in the end.
    bool expect_success;

    // Expected times the url should be requested.
    uint32_t expected_num_requests;
  } const kTestCases[] = {
      // No retry on 5xx when retries disabled.
      {0, SimpleURLLoader::RETRY_NEVER, 1, false, 1},

      // No retry on 5xx when retries enabled on network change.
      {1, SimpleURLLoader::RETRY_ON_NETWORK_CHANGE, 1, false, 1},

      // As many retries allowed as 5xx errors.
      {1, SimpleURLLoader::RETRY_ON_5XX, 1, true, 2},
      {1,
       SimpleURLLoader::RETRY_ON_5XX | SimpleURLLoader::RETRY_ON_NETWORK_CHANGE,
       1, true, 2},
      {2, SimpleURLLoader::RETRY_ON_5XX, 2, true, 3},

      // More retries than 5xx errors.
      {2, SimpleURLLoader::RETRY_ON_5XX, 1, true, 2},

      // Fewer retries than 5xx errors.
      {1, SimpleURLLoader::RETRY_ON_5XX, 2, false, 2},
  };

  for (const auto& test_case : kTestCases) {
    MockURLLoaderFactory loader_factory(&scoped_task_environment_);
    for (int i = 0; i < test_case.num_5xx; i++) {
      loader_factory.AddEvents({TestLoaderEvent::kReceived501Response});
    }

    // Valid response with a 1-byte body.
    loader_factory.AddEvents({TestLoaderEvent::kReceivedResponse,
                              TestLoaderEvent::kBodyBufferReceived,
                              TestLoaderEvent::kBodyDataRead,
                              TestLoaderEvent::kBodyBufferClosed,
                              TestLoaderEvent::kResponseComplete});

    SimpleLoaderTestHelper test_helper(GetParam());
    test_helper.simple_url_loader()->SetRetryOptions(test_case.max_retries,
                                                     test_case.retry_mode);
    test_helper.RunRequestForURL(&loader_factory, GURL(kInitialURL));

    if (test_case.expect_success) {
      EXPECT_EQ(net::OK, test_helper.simple_url_loader()->NetError());
      EXPECT_EQ(200, test_helper.GetResponseCode());
      ASSERT_TRUE(test_helper.response_body());
      EXPECT_EQ(1u, test_helper.response_body()->size());
    } else {
      EXPECT_EQ(501, test_helper.GetResponseCode());
      EXPECT_FALSE(test_helper.response_body());
    }

    EXPECT_EQ(test_case.expected_num_requests,
              loader_factory.requested_urls().size());
    for (const auto& url : loader_factory.requested_urls()) {
      EXPECT_EQ(kInitialURL, url);
    }
  }
}

// Test that when retrying on 5xx is enabled, there's no retry on a 4xx error.
TEST_P(SimpleURLLoaderTest, NoRetryOn4xx) {
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents({TestLoaderEvent::kReceived401Response});
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyBufferReceived,
       TestLoaderEvent::kBodyBufferClosed, TestLoaderEvent::kResponseComplete});

  test_helper()->simple_url_loader()->SetRetryOptions(
      1, SimpleURLLoader::RETRY_ON_5XX);
  test_helper()->RunRequestForURL(&loader_factory, GURL("foo://bar/"));

  EXPECT_EQ(401, test_helper()->GetResponseCode());
  EXPECT_FALSE(test_helper()->response_body());
  EXPECT_EQ(1u, loader_factory.requested_urls().size());
}

// Checks that retrying after a redirect works. The original URL should be
// re-requested.
TEST_P(SimpleURLLoaderTest, RetryAfterRedirect) {
  const GURL kInitialURL("foo://bar/initial");
  MockURLLoaderFactory loader_factory(&scoped_task_environment_);
  loader_factory.AddEvents({TestLoaderEvent::kReceivedRedirect,
                            TestLoaderEvent::kReceived501Response});
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedRedirect, TestLoaderEvent::kReceivedResponse,
       TestLoaderEvent::kBodyBufferReceived, TestLoaderEvent::kBodyBufferClosed,
       TestLoaderEvent::kResponseComplete});

  test_helper()->simple_url_loader()->SetRetryOptions(
      1, SimpleURLLoader::RETRY_ON_5XX);
  test_helper()->RunRequestForURL(&loader_factory, kInitialURL);

  EXPECT_EQ(200, test_helper()->GetResponseCode());
  EXPECT_TRUE(test_helper()->response_body());

  EXPECT_EQ(2u, loader_factory.requested_urls().size());
  for (const auto& url : loader_factory.requested_urls()) {
    EXPECT_EQ(kInitialURL, url);
  }
}

TEST_P(SimpleURLLoaderTest, RetryOnNetworkChange) {
  // TestLoaderEvents up to (and including) a network change. Since
  // SimpleURLLoader always waits for the body buffer to be closed before
  // retrying, everything that has a kBodyBufferReceived message must also have
  // a kBodyBufferClosed message. Each test case will be tried against each of
  // these event sequences.
  const std::vector<std::vector<TestLoaderEvent>> kNetworkChangedEvents = {
      {TestLoaderEvent::kResponseCompleteNetworkChanged},
      {TestLoaderEvent::kReceivedResponse,
       TestLoaderEvent::kResponseCompleteNetworkChanged},
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyBufferReceived,
       TestLoaderEvent::kBodyBufferClosed,
       TestLoaderEvent::kResponseCompleteNetworkChanged},
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyBufferReceived,
       TestLoaderEvent::kResponseCompleteNetworkChanged,
       TestLoaderEvent::kBodyBufferClosed},
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyBufferReceived,
       TestLoaderEvent::kBodyDataRead, TestLoaderEvent::kBodyBufferClosed,
       TestLoaderEvent::kResponseCompleteNetworkChanged},
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyBufferReceived,
       TestLoaderEvent::kBodyDataRead,
       TestLoaderEvent::kResponseCompleteNetworkChanged,
       TestLoaderEvent::kBodyBufferClosed},
      {TestLoaderEvent::kReceivedRedirect,
       TestLoaderEvent::kResponseCompleteNetworkChanged},
  };

  const GURL kInitialURL("foo://bar/initial");

  // Test cases in which to try each entry in kNetworkChangedEvents.
  struct TestCase {
    // Parameters passed to SetRetryOptions.
    int max_retries;
    int retry_mode;

    // Number of network changes responses before a successful response.
    // For each network change, the entire sequence of an entry in
    // kNetworkChangedEvents is repeated.
    int num_network_changes;

    // Whether the request is expected to succeed in the end.
    bool expect_success;

    // Expected times the url should be requested.
    uint32_t expected_num_requests;
  } const kTestCases[] = {
      // No retry on network change when retries disabled.
      {0, SimpleURLLoader::RETRY_NEVER, 1, false, 1},

      // No retry on network change when retries enabled on 5xx response.
      {1, SimpleURLLoader::RETRY_ON_5XX, 1, false, 1},

      // As many retries allowed as network changes.
      {1, SimpleURLLoader::RETRY_ON_NETWORK_CHANGE, 1, true, 2},
      {1,
       SimpleURLLoader::RETRY_ON_NETWORK_CHANGE | SimpleURLLoader::RETRY_ON_5XX,
       1, true, 2},
      {2, SimpleURLLoader::RETRY_ON_NETWORK_CHANGE, 2, true, 3},

      // More retries than network changes.
      {2, SimpleURLLoader::RETRY_ON_NETWORK_CHANGE, 1, true, 2},

      // Fewer retries than network changes.
      {1, SimpleURLLoader::RETRY_ON_NETWORK_CHANGE, 2, false, 2},
  };

  for (const auto& network_events : kNetworkChangedEvents) {
    for (const auto& test_case : kTestCases) {
      MockURLLoaderFactory loader_factory(&scoped_task_environment_);
      for (int i = 0; i < test_case.num_network_changes; i++) {
        loader_factory.AddEvents(network_events);
      }

      // Valid response with a 1-byte body.
      loader_factory.AddEvents({TestLoaderEvent::kReceivedResponse,
                                TestLoaderEvent::kBodyBufferReceived,
                                TestLoaderEvent::kBodyDataRead,
                                TestLoaderEvent::kBodyBufferClosed,
                                TestLoaderEvent::kResponseComplete});

      SimpleLoaderTestHelper test_helper(GetParam());
      test_helper.simple_url_loader()->SetRetryOptions(test_case.max_retries,
                                                       test_case.retry_mode);
      test_helper.RunRequestForURL(&loader_factory, GURL(kInitialURL));

      if (test_case.expect_success) {
        EXPECT_EQ(net::OK, test_helper.simple_url_loader()->NetError());
        EXPECT_EQ(200, test_helper.GetResponseCode());
        ASSERT_TRUE(test_helper.response_body());
        EXPECT_EQ(1u, test_helper.response_body()->size());
      } else {
        EXPECT_EQ(net::ERR_NETWORK_CHANGED,
                  test_helper.simple_url_loader()->NetError());
        EXPECT_FALSE(test_helper.response_body());
      }

      EXPECT_EQ(test_case.expected_num_requests,
                loader_factory.requested_urls().size());
      for (const auto& url : loader_factory.requested_urls()) {
        EXPECT_EQ(kInitialURL, url);
      }
    }
  }

  // Check that there's no retry for each entry in kNetworkChangedEvents when an
  // error other than a network change is received.
  for (const auto& network_events : kNetworkChangedEvents) {
    std::vector<TestLoaderEvent> modifed_network_events = network_events;
    for (auto& test_loader_event : modifed_network_events) {
      if (test_loader_event == TestLoaderEvent::kResponseCompleteNetworkChanged)
        test_loader_event = TestLoaderEvent::kResponseCompleteFailed;
    }
    MockURLLoaderFactory loader_factory(&scoped_task_environment_);
    loader_factory.AddEvents(modifed_network_events);
    // Valid response, which should never be read.
    loader_factory.AddEvents({TestLoaderEvent::kReceivedResponse,
                              TestLoaderEvent::kBodyBufferReceived,
                              TestLoaderEvent::kBodyBufferClosed,
                              TestLoaderEvent::kResponseComplete});

    SimpleLoaderTestHelper test_helper(GetParam());
    test_helper.simple_url_loader()->SetRetryOptions(
        1, SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
    test_helper.RunRequestForURL(&loader_factory, GURL(kInitialURL));

    EXPECT_EQ(net::ERR_TIMED_OUT, test_helper.simple_url_loader()->NetError());
    EXPECT_FALSE(test_helper.response_body());
    EXPECT_EQ(1u, loader_factory.requested_urls().size());
  }
}

INSTANTIATE_TEST_CASE_P(
    /* No prefix */,
    SimpleURLLoaderTest,
    testing::Values(SimpleLoaderTestHelper::DownloadType::TO_STRING,
                    SimpleLoaderTestHelper::DownloadType::TO_FILE));

class SimpleURLLoaderFileTest : public SimpleURLLoaderTestBase,
                                public testing::Test {
 public:
  SimpleURLLoaderFileTest()
      : test_helper_(SimpleLoaderTestHelper::DownloadType::TO_FILE) {}
  ~SimpleURLLoaderFileTest() override {}

  SimpleLoaderTestHelper* test_helper() { return &test_helper_; }

 private:
  SimpleLoaderTestHelper test_helper_;
};

// Make sure that an existing file will be completely overwritten.
TEST_F(SimpleURLLoaderFileTest, OverwriteFile) {
  std::string junk_data(100, '!');
  ASSERT_EQ(static_cast<int>(junk_data.size()),
            base::WriteFile(test_helper()->dest_path(), junk_data.data(),
                            junk_data.size()));
  ASSERT_TRUE(base::PathExists(test_helper()->dest_path()));

  ResourceRequest resource_request;
  resource_request.url = GURL("data:text/plain,foo");
  test_helper()->RunRequest(url_loader_factory_.get(), resource_request);

  EXPECT_EQ(net::OK, test_helper()->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper()->simple_url_loader()->ResponseInfo());
  ASSERT_TRUE(test_helper()->response_body());
  EXPECT_EQ("foo", *test_helper()->response_body());
}

// Make sure that file creation errors are handled correctly.
TEST_F(SimpleURLLoaderFileTest, FileCreateError) {
  ASSERT_TRUE(base::CreateDirectory(test_helper()->dest_path()));
  ASSERT_TRUE(base::PathExists(test_helper()->dest_path()));

  ResourceRequest resource_request;
  resource_request.url = GURL("data:text/plain,foo");
  // The directory should still exist after the download fails.
  test_helper()->set_expect_path_exists_on_error(true);
  test_helper()->RunRequest(url_loader_factory_.get(), resource_request);

  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            test_helper()->simple_url_loader()->NetError());
  EXPECT_TRUE(test_helper()->simple_url_loader()->ResponseInfo());
  EXPECT_FALSE(test_helper()->response_body());
}

// Make sure that destroying the loader destroys a partially downloaded file.
TEST_F(SimpleURLLoaderFileTest, DeleteLoaderDuringRequestDestroysFile) {
  for (bool body_data_read : {false, true}) {
    for (bool body_buffer_closed : {false, true}) {
      for (bool client_pipe_closed : {false, true}) {
        // If both pipes were closed cleanly, the file shouldn't be deleted, as
        // that indicates success.
        if (body_buffer_closed && client_pipe_closed)
          continue;

        MockURLLoaderFactory loader_factory(&scoped_task_environment_);
        std::vector<TestLoaderEvent> events;
        events.push_back(TestLoaderEvent::kReceivedResponse);
        events.push_back(TestLoaderEvent::kBodyBufferReceived);
        if (body_data_read)
          events.push_back(TestLoaderEvent::kBodyDataRead);
        if (body_buffer_closed)
          events.push_back(TestLoaderEvent::kBodyBufferClosed);
        if (client_pipe_closed)
          events.push_back(TestLoaderEvent::kClientPipeClosed);
        loader_factory.AddEvents(events);

        // The request just hangs after receiving some body data.
        ResourceRequest resource_request;
        resource_request.url = GURL("foo://bar/");

        std::unique_ptr<SimpleLoaderTestHelper> test_helper =
            std::make_unique<SimpleLoaderTestHelper>(
                SimpleLoaderTestHelper::DownloadType::TO_FILE);
        test_helper->StartRequest(&loader_factory, resource_request);

        // Wait for the request to advance as far as it's going to.
        scoped_task_environment_.RunUntilIdle();

        // Destination file should have been created, and request should still
        // be in progress.
        base::FilePath dest_path = test_helper->dest_path();
        EXPECT_TRUE(base::PathExists(dest_path));
        EXPECT_FALSE(test_helper->done());

        // Destroying the SimpleURLLoader now should post a task to destroy the
        // file.
        test_helper.reset();
        scoped_task_environment_.RunUntilIdle();
        EXPECT_FALSE(base::PathExists(dest_path));
      }
    }
  }
}

}  // namespace
}  // namespace content
