// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>
#include <list>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/network/network_context.h"
#include "content/network/url_loader.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/controllable_http_response.h"
#include "content/public/test/test_url_loader_client.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/mime_sniffer.h"
#include "net/base/net_errors.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_job.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/test/test_data_pipe_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace content {

namespace {

static network::ResourceRequest CreateResourceRequest(const char* method,
                                                      const GURL& url) {
  network::ResourceRequest request;
  request.method = std::string(method);
  request.url = url;
  request.site_for_cookies = url;  // bypass third-party cookie blocking
  request.request_initiator =
      url::Origin::Create(url);  // ensure initiator is set
  request.is_main_frame = true;
  request.transition_type = ui::PAGE_TRANSITION_LINK;
  request.allow_download = true;
  return request;
}

class URLRequestMultipleWritesJob : public net::URLRequestJob {
 public:
  URLRequestMultipleWritesJob(net::URLRequest* request,
                              net::NetworkDelegate* network_delegate,
                              std::list<std::string> packets,
                              net::Error net_error,
                              bool async_reads)
      : URLRequestJob(request, network_delegate),
        packets_(std::move(packets)),
        net_error_(net_error),
        async_reads_(async_reads),
        weak_factory_(this) {}

  // net::URLRequestJob implementation:
  void Start() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&URLRequestMultipleWritesJob::StartAsync,
                                  weak_factory_.GetWeakPtr()));
  }

  int ReadRawData(net::IOBuffer* buf, int buf_size) override {
    int result;
    if (packets_.empty()) {
      result = net_error_;
    } else {
      std::string packet = packets_.front();
      packets_.pop_front();
      CHECK_GE(buf_size, static_cast<int>(packet.length()));
      memcpy(buf->data(), packet.c_str(), packet.length());
      result = packet.length();
    }

    if (async_reads_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&URLRequestMultipleWritesJob::ReadRawDataComplete,
                         weak_factory_.GetWeakPtr(), result));
      return net::ERR_IO_PENDING;
    }
    return result;
  }

 private:
  ~URLRequestMultipleWritesJob() override {}

  void StartAsync() { NotifyHeadersComplete(); }

  std::list<std::string> packets_;
  net::Error net_error_;
  bool async_reads_;

  base::WeakPtrFactory<URLRequestMultipleWritesJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestMultipleWritesJob);
};

class MultipleWritesInterceptor : public net::URLRequestInterceptor {
 public:
  MultipleWritesInterceptor(std::list<std::string> packets,
                            net::Error net_error,
                            bool async_reads)
      : packets_(std::move(packets)),
        net_error_(net_error),
        async_reads_(async_reads) {}
  ~MultipleWritesInterceptor() override {}

  static GURL GetURL() { return GURL("http://foo"); }

  // URLRequestInterceptor implementation:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new URLRequestMultipleWritesJob(request, network_delegate,
                                           std::move(packets_), net_error_,
                                           async_reads_);
  }

 private:
  std::list<std::string> packets_;
  net::Error net_error_;
  bool async_reads_;

  DISALLOW_COPY_AND_ASSIGN(MultipleWritesInterceptor);
};

class RequestInterceptor : public net::URLRequestInterceptor {
 public:
  using InterceptCallback = base::Callback<void(net::URLRequest*)>;

  explicit RequestInterceptor(const InterceptCallback& callback)
      : callback_(callback) {}
  ~RequestInterceptor() override {}

  // URLRequestInterceptor implementation:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    callback_.Run(request);
    return nullptr;
  }

 private:
  InterceptCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(RequestInterceptor);
};

}  // namespace

class URLLoaderTest : public testing::Test {
 public:
  URLLoaderTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        context_(NetworkContext::CreateForTesting()) {
    net::URLRequestFailedJob::AddUrlHandler();
  }
  ~URLLoaderTest() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  void SetUp() override {
    test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    // This Unretained is safe because test_server_ is owned by |this|.
    test_server_.RegisterRequestMonitor(
        base::Bind(&URLLoaderTest::Monitor, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());
  }

  // Attempts to load |url| and returns the resulting error code. If |body| is
  // non-NULL, also attempts to read the response body. The advantage of using
  // |body| instead of calling ReadBody() after Load is that it will load the
  // response body before URLLoader is complete, so URLLoader completion won't
  // block on trying to write the body buffer.
  int Load(const GURL& url, std::string* body = nullptr) WARN_UNUSED_RESULT {
    DCHECK(!ran_);
    network::mojom::URLLoaderPtr loader;

    network::ResourceRequest request =
        CreateResourceRequest(!request_body_ ? "GET" : "POST", url);
    uint32_t options = network::mojom::kURLLoadOptionNone;
    if (send_ssl_with_response_)
      options |= network::mojom::kURLLoadOptionSendSSLInfoWithResponse;
    if (sniff_)
      options |= network::mojom::kURLLoadOptionSniffMimeType;
    if (send_ssl_for_cert_error_)
      options |= network::mojom::kURLLoadOptionSendSSLInfoForCertificateError;

    if (request_body_)
      request.request_body = request_body_;

    URLLoader loader_impl(context(), mojo::MakeRequest(&loader), options,
                          request, false, client_.CreateInterfacePtr(),
                          TRAFFIC_ANNOTATION_FOR_TESTS, 0);

    ran_ = true;

    if (expect_redirect_) {
      client_.RunUntilRedirectReceived();
      loader->FollowRedirect();
    }

    if (body) {
      client_.RunUntilResponseBodyArrived();
      *body = ReadBody();
    }

    client_.RunUntilComplete();
    if (body) {
      EXPECT_EQ(body->size(),
                static_cast<size_t>(
                    client()->completion_status().decoded_body_length));
    }

    return client_.completion_status().error_code;
  }

  void LoadAndCompareFile(const std::string& path) {
    base::FilePath file = GetTestFilePath(path);

    std::string expected;
    if (!base::ReadFileToString(file, &expected)) {
      ADD_FAILURE() << "File not found: " << file.value();
      return;
    }

    std::string body;
    EXPECT_EQ(net::OK,
              Load(test_server()->GetURL(std::string("/") + path), &body));
    EXPECT_EQ(expected, body);
    // The file isn't compressed, so both encoded and decoded body lengths
    // should match the read body length.
    EXPECT_EQ(
        expected.size(),
        static_cast<size_t>(client()->completion_status().decoded_body_length));
    EXPECT_EQ(
        expected.size(),
        static_cast<size_t>(client()->completion_status().encoded_body_length));
    // Over the wire length should include headers, so should be longer.
    // TODO(mmenke): Worth adding better tests for encoded_data_length?
    EXPECT_LT(
        expected.size(),
        static_cast<size_t>(client()->completion_status().encoded_data_length));
  }

  // Adds a MultipleWritesInterceptor for MultipleWritesInterceptor::GetURL()
  // that results in seeing each element of |packets| read individually, and
  // then a final read that returns |net_error|. The URLRequestInterceptor is
  // not removed from URLFilter until the test fixture is torn down.
  // |async_reads| indicates whether all reads (including those for |packets|
  // and |net_error|) complete asynchronously or not.
  void AddMultipleWritesInterceptor(std::list<std::string> packets,
                                    net::Error net_error,
                                    bool async_reads) {
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        MultipleWritesInterceptor::GetURL(),
        std::unique_ptr<net::URLRequestInterceptor>(
            new MultipleWritesInterceptor(std::move(packets), net_error,
                                          async_reads)));
  }

  // If |second| is empty, then it's ignored.
  void LoadPacketsAndVerifyContents(const std::string& first,
                                    const std::string& second) {
    EXPECT_FALSE(first.empty());
    std::list<std::string> packets;
    packets.push_back(first);
    if (!second.empty())
      packets.push_back(second);
    AddMultipleWritesInterceptor(std::move(packets), net::OK,
                                 false /* async_reads */);

    std::string expected_body = first + second;
    std::string actual_body;
    EXPECT_EQ(net::OK, Load(MultipleWritesInterceptor::GetURL(), &actual_body));

    EXPECT_EQ(actual_body, expected_body);
  }

  // Adds an interceptor that can examine the URLRequest object.
  void AddRequestObserver(
      const GURL& url,
      const RequestInterceptor::InterceptCallback& callback) {
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        url, std::unique_ptr<net::URLRequestInterceptor>(
                 new RequestInterceptor(callback)));
  }

  net::EmbeddedTestServer* test_server() { return &test_server_; }
  NetworkContext* context() { return context_.get(); }
  TestURLLoaderClient* client() { return &client_; }
  void DestroyContext() { context_.reset(); }

  // Returns the path of the requested file in the test data directory.
  base::FilePath GetTestFilePath(const std::string& file_name) {
    base::FilePath file_path;
    PathService::Get(DIR_TEST_DATA, &file_path);
    return file_path.AppendASCII(file_name);
  }

  base::File OpenFileForUpload(const base::FilePath& file_path) {
    int open_flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
#if defined(OS_WIN)
    open_flags |= base::File::FLAG_ASYNC;
#endif  //  defined(OS_WIN)
    base::File file(file_path, open_flags);
    EXPECT_TRUE(file.IsValid());
    return file;
  }

  // Configure how Load() works.
  void set_sniff() {
    DCHECK(!ran_);
    sniff_ = true;
  }
  void set_send_ssl_with_response() {
    DCHECK(!ran_);
    send_ssl_with_response_ = true;
  }
  void set_send_ssl_for_cert_error() {
    DCHECK(!ran_);
    send_ssl_for_cert_error_ = true;
  }
  void set_expect_redirect() {
    DCHECK(!ran_);
    expect_redirect_ = true;
  }
  void set_request_body(
      scoped_refptr<network::ResourceRequestBody> request_body) {
    request_body_ = request_body;
  }

  // Convenience methods after calling Load();
  std::string mime_type() const {
    DCHECK(ran_);
    return client_.response_head().mime_type;
  }
  const base::Optional<net::SSLInfo>& ssl_info() const {
    DCHECK(ran_);
    return client_.ssl_info();
  }

  // Reads the response body from client()->response_body() until the channel is
  // closed. Expects client()->response_body() to already be populated, and
  // non-NULL.
  std::string ReadBody() {
    std::string body;
    while (true) {
      MojoHandle consumer = client()->response_body().value();

      const void* buffer;
      uint32_t num_bytes;
      MojoResult rv = MojoBeginReadData(consumer, &buffer, &num_bytes,
                                        MOJO_READ_DATA_FLAG_NONE);
      // If no data has been received yet, spin the message loop until it has.
      if (rv == MOJO_RESULT_SHOULD_WAIT) {
        mojo::SimpleWatcher watcher(
            FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
        base::RunLoop run_loop;

        watcher.Watch(
            client()->response_body(),
            MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            MOJO_WATCH_CONDITION_SATISFIED,
            base::BindRepeating(
                [](base::Closure quit, MojoResult result,
                   const mojo::HandleSignalsState& state) { quit.Run(); },
                run_loop.QuitClosure()));
        run_loop.Run();
        continue;
      }

      // The pipe was closed.
      if (rv == MOJO_RESULT_FAILED_PRECONDITION)
        return body;

      CHECK_EQ(rv, MOJO_RESULT_OK);

      body.append(static_cast<const char*>(buffer), num_bytes);
      MojoEndReadData(consumer, num_bytes);
    }

    return body;
  }

  std::string ReadAvailableBody() {
    MojoHandle consumer = client()->response_body().value();

    uint32_t num_bytes = 0;
    MojoResult result =
        MojoReadData(consumer, nullptr, &num_bytes, MOJO_READ_DATA_FLAG_QUERY);
    CHECK_EQ(MOJO_RESULT_OK, result);
    if (num_bytes == 0)
      return std::string();

    std::vector<char> buffer(num_bytes);
    result = MojoReadData(consumer, buffer.data(), &num_bytes,
                          MOJO_READ_DATA_FLAG_NONE);
    CHECK_EQ(MOJO_RESULT_OK, result);
    CHECK_EQ(num_bytes, buffer.size());

    return std::string(buffer.data(), buffer.size());
  }

  const net::test_server::HttpRequest& sent_request() const {
    return sent_request_;
  }

 private:
  void Monitor(const net::test_server::HttpRequest& request) {
    sent_request_ = request;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<NetworkContext> context_;

  // Options applied to the created request in Load().
  bool sniff_ = false;
  bool send_ssl_with_response_ = false;
  bool send_ssl_for_cert_error_ = false;
  bool expect_redirect_ = false;
  scoped_refptr<network::ResourceRequestBody> request_body_;

  // Used to ensure that methods are called either before or after a request is
  // made, since the test fixture is meant to be used only once.
  bool ran_ = false;
  net::test_server::HttpRequest sent_request_;
  TestURLLoaderClient client_;
};

TEST_F(URLLoaderTest, Basic) {
  LoadAndCompareFile("simple_page.html");
}

TEST_F(URLLoaderTest, Empty) {
  LoadAndCompareFile("empty.html");
}

TEST_F(URLLoaderTest, BasicSSL) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  GURL url = https_server.GetURL("/simple_page.html");
  set_send_ssl_with_response();
  EXPECT_EQ(net::OK, Load(url));
  ASSERT_TRUE(!!ssl_info());
  ASSERT_TRUE(!!ssl_info()->cert);

  ASSERT_TRUE(https_server.GetCertificate()->Equals(ssl_info()->cert.get()));
}

TEST_F(URLLoaderTest, SSLSentOnlyWhenRequested) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  GURL url = https_server.GetURL("/simple_page.html");
  EXPECT_EQ(net::OK, Load(url));
  ASSERT_FALSE(!!ssl_info());
}

// Test decoded_body_length / encoded_body_length when they're different.
TEST_F(URLLoaderTest, GzipTest) {
  std::string body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/gzip-body?Body"), &body));
  EXPECT_EQ("Body", body);
  // Deflating a 4-byte string should result in a longer string - main thing to
  // check here, though, is that the two lengths are of different.
  EXPECT_LT(client()->completion_status().decoded_body_length,
            client()->completion_status().encoded_body_length);
  // Over the wire length should include headers, so should be longer.
  EXPECT_LT(client()->completion_status().encoded_body_length,
            client()->completion_status().encoded_data_length);
}

TEST_F(URLLoaderTest, ErrorBeforeHeaders) {
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE,
            Load(test_server()->GetURL("/close-socket"), nullptr));
  EXPECT_FALSE(client()->response_body().is_valid());
}

TEST_F(URLLoaderTest, SyncErrorWhileReadingBody) {
  std::string body;
  EXPECT_EQ(net::ERR_FAILED,
            Load(net::URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
                     net::URLRequestFailedJob::READ_SYNC, net::ERR_FAILED),
                 &body));
  EXPECT_EQ("", body);
}

TEST_F(URLLoaderTest, AsyncErrorWhileReadingBody) {
  std::string body;
  EXPECT_EQ(net::ERR_FAILED,
            Load(net::URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
                     net::URLRequestFailedJob::READ_ASYNC, net::ERR_FAILED),
                 &body));
  EXPECT_EQ("", body);
}

TEST_F(URLLoaderTest, SyncErrorWhileReadingBodyAfterBytesReceived) {
  const std::string kBody("Foo.");

  std::list<std::string> packets;
  packets.push_back(kBody);
  AddMultipleWritesInterceptor(packets, net::ERR_ACCESS_DENIED,
                               false /*async_reads*/);
  std::string body;
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            Load(MultipleWritesInterceptor::GetURL(), &body));
  EXPECT_EQ(kBody, body);
}

TEST_F(URLLoaderTest, AsyncErrorWhileReadingBodyAfterBytesReceived) {
  const std::string kBody("Foo.");

  std::list<std::string> packets;
  packets.push_back(kBody);
  AddMultipleWritesInterceptor(packets, net::ERR_ACCESS_DENIED,
                               true /*async_reads*/);
  std::string body;
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            Load(MultipleWritesInterceptor::GetURL(), &body));
  EXPECT_EQ(kBody, body);
}

TEST_F(URLLoaderTest, DestroyContextWithLiveRequest) {
  GURL url = test_server()->GetURL("/hung-after-headers");
  network::ResourceRequest request = CreateResourceRequest("GET", url);

  network::mojom::URLLoaderPtr loader;
  // The loader is implicitly owned by the client and the NetworkContext, so
  // don't hold on to a pointer to it.
  base::WeakPtr<URLLoader> loader_impl =
      (new URLLoader(context(), mojo::MakeRequest(&loader), 0, request, false,
                     client()->CreateInterfacePtr(),
                     TRAFFIC_ANNOTATION_FOR_TESTS, 0))
          ->GetWeakPtrForTests();

  client()->RunUntilResponseReceived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  // Request hasn't completed, so the loader should not have been destroyed.
  EXPECT_TRUE(loader_impl);

  // Destroying the context should result in destroying the loader and the
  // client receiving a connection error.
  DestroyContext();
  EXPECT_FALSE(loader_impl);

  client()->RunUntilConnectionError();
  EXPECT_FALSE(client()->has_received_completion());
  EXPECT_EQ(0u, client()->download_data_length());
}

TEST_F(URLLoaderTest, DoNotSniffUnlessSpecified) {
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test0.html")));
  ASSERT_TRUE(mime_type().empty());
}

TEST_F(URLLoaderTest, SniffMimeType) {
  set_sniff();
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test0.html")));
  ASSERT_EQ(std::string("text/html"), mime_type());
}

TEST_F(URLLoaderTest, RespectNoSniff) {
  set_sniff();
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/nosniff-test.html")));
  ASSERT_TRUE(mime_type().empty());
}

TEST_F(URLLoaderTest, DoNotSniffHTMLFromTextPlain) {
  set_sniff();
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test1.html")));
  ASSERT_EQ(std::string("text/plain"), mime_type());
}

TEST_F(URLLoaderTest, DoNotSniffHTMLFromImageGIF) {
  set_sniff();
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test2.html")));
  ASSERT_EQ(std::string("image/gif"), mime_type());
}

TEST_F(URLLoaderTest, EmptyHtmlIsTextPlain) {
  set_sniff();
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test4.html")));
  ASSERT_EQ(std::string("text/plain"), mime_type());
}

TEST_F(URLLoaderTest, EmptyHtmlIsTextPlainWithAsyncResponse) {
  set_sniff();

  const std::string kBody;

  std::list<std::string> packets;
  packets.push_back(kBody);
  AddMultipleWritesInterceptor(packets, net::OK, true /*async_reads*/);

  std::string body;
  EXPECT_EQ(net::OK, Load(MultipleWritesInterceptor::GetURL(), &body));
  EXPECT_EQ(kBody, body);
  ASSERT_EQ(std::string("text/plain"), mime_type());
}

// Tests the case where the first read doesn't have enough data to figure out
// the right mime type. The second read would have enough data even though the
// total bytes is still smaller than net::kMaxBytesToSniff.
TEST_F(URLLoaderTest, FirstReadNotEnoughToSniff1) {
  set_sniff();
  std::string first(500, 'a');
  std::string second(std::string(100, 'b'));
  second[10] = 0;
  EXPECT_LE(first.size() + second.size(),
            static_cast<uint32_t>(net::kMaxBytesToSniff));
  LoadPacketsAndVerifyContents(first, second);
  ASSERT_EQ(std::string("application/octet-stream"), mime_type());
}

// Like above, except that the total byte count is > kMaxBytesToSniff.
TEST_F(URLLoaderTest, FirstReadNotEnoughToSniff2) {
  set_sniff();
  std::string first(500, 'a');
  std::string second(std::string(1000, 'b'));
  second[10] = 0;
  EXPECT_GE(first.size() + second.size(),
            static_cast<uint32_t>(net::kMaxBytesToSniff));
  LoadPacketsAndVerifyContents(first, second);
  ASSERT_EQ(std::string("application/octet-stream"), mime_type());
}

// Tests that even if the first and only read is smaller than the minimum number
// of bytes needed to sniff, the loader works correctly and returns the data.
TEST_F(URLLoaderTest, LoneReadNotEnoughToSniff) {
  set_sniff();
  std::string first(net::kMaxBytesToSniff - 100, 'a');
  LoadPacketsAndVerifyContents(first, std::string());
  ASSERT_EQ(std::string("text/plain"), mime_type());
}

// Tests the simple case where the first read is enough to sniff.
TEST_F(URLLoaderTest, FirstReadIsEnoughToSniff) {
  set_sniff();
  std::string first(net::kMaxBytesToSniff + 100, 'a');
  LoadPacketsAndVerifyContents(first, std::string());
  ASSERT_EQ(std::string("text/plain"), mime_type());
}

class NeverFinishedBodyHttpResponse : public net::test_server::HttpResponse {
 public:
  NeverFinishedBodyHttpResponse() = default;
  ~NeverFinishedBodyHttpResponse() override = default;

 private:
  // net::test_server::HttpResponse implementation.
  void SendResponse(
      const net::test_server::SendBytesCallback& send,
      const net::test_server::SendCompleteCallback& done) override {
    send.Run(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n\r\n"
        "long long ago..." +
            std::string(1024 * 1024, 'a'),
        base::Bind([]() {}));

    // Never call |done|, so the other side will never see the completion of the
    // response body.
  }
};

TEST_F(URLLoaderTest, CloseResponseBodyConsumerBeforeProducer) {
  net::EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::Bind([](const net::test_server::HttpRequest& request) {
        std::unique_ptr<net::test_server::HttpResponse> response =
            std::make_unique<NeverFinishedBodyHttpResponse>();
        return response;
      }));
  ASSERT_TRUE(server.Start());

  network::ResourceRequest request =
      CreateResourceRequest("GET", server.GetURL("/hello.html"));

  network::mojom::URLLoaderPtr loader;
  // The loader is implicitly owned by the client and the NetworkContext.
  new URLLoader(context(), mojo::MakeRequest(&loader), 0, request, false,
                client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS,
                0);

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  // Wait for a little amount of time for the response body pipe to be filled.
  // (Please note that this doesn't guarantee that the pipe is filled to the
  // point that it is not writable anymore.)
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(100));
  run_loop.Run();

  auto response_body = client()->response_body_release();
  response_body.reset();
  loader.reset();

  // The client is disconnected only when the other side observes that both the
  // URLLoaderPtr and the response body pipe are disconnected.
  client()->RunUntilConnectionError();

  EXPECT_FALSE(client()->has_received_completion());
}

TEST_F(URLLoaderTest, PauseReadingBodyFromNetBeforeRespnoseHeaders) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContents = "This is the data as you requested.";

  net::EmbeddedTestServer server;
  ControllableHttpResponse response_controller(&server, kPath);
  ASSERT_TRUE(server.Start());

  network::ResourceRequest request =
      CreateResourceRequest("GET", server.GetURL(kPath));

  network::mojom::URLLoaderPtr loader;
  // The loader is implicitly owned by the client and the NetworkContext.
  new URLLoader(context(), mojo::MakeRequest(&loader), 0, request, false,
                client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS,
                0);

  // Pausing reading response body from network stops future reads from the
  // underlying URLRequest. So no data should be sent using the response body
  // data pipe.
  loader->PauseReadingBodyFromNet();
  // In order to avoid flakiness, make sure PauseReadBodyFromNet() is handled by
  // the loader before the test HTTP server serves the response.
  loader.FlushForTesting();

  response_controller.WaitForRequest();
  response_controller.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n\r\n" +
      std::string(kBodyContents));
  response_controller.Done();

  // We will still receive the response body data pipe, although there won't be
  // any data available until ResumeReadBodyFromNet() is called.
  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  // Wait for a little amount of time so that if the loader mistakenly reads
  // response body from the underlying URLRequest, it is easier to find out.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(100));
  run_loop.Run();

  std::string available_data = ReadAvailableBody();
  EXPECT_TRUE(available_data.empty());

  loader->ResumeReadingBodyFromNet();
  client()->RunUntilComplete();

  available_data = ReadBody();
  EXPECT_EQ(kBodyContents, available_data);
}

TEST_F(URLLoaderTest, PauseReadingBodyFromNetWhenReadIsPending) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContentsFirstHalf = "This is the first half.";
  const char* const kBodyContentsSecondHalf = "This is the second half.";

  net::EmbeddedTestServer server;
  ControllableHttpResponse response_controller(&server, kPath);
  ASSERT_TRUE(server.Start());

  network::ResourceRequest request =
      CreateResourceRequest("GET", server.GetURL(kPath));

  network::mojom::URLLoaderPtr loader;
  // The loader is implicitly owned by the client and the NetworkContext.
  new URLLoader(context(), mojo::MakeRequest(&loader), 0, request, false,
                client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS,
                0);

  response_controller.WaitForRequest();
  response_controller.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n\r\n" +
      std::string(kBodyContentsFirstHalf));

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  loader->PauseReadingBodyFromNet();
  loader.FlushForTesting();

  response_controller.Send(kBodyContentsSecondHalf);
  response_controller.Done();

  // It is uncertain how much data has been read before reading is actually
  // paused, because if there is a pending read when PauseReadingBodyFromNet()
  // arrives, the pending read won't be cancelled. Therefore, this test only
  // checks that after ResumeReadingBodyFromNet() we should be able to get the
  // whole response body.
  loader->ResumeReadingBodyFromNet();
  client()->RunUntilComplete();

  EXPECT_EQ(std::string(kBodyContentsFirstHalf) +
                std::string(kBodyContentsSecondHalf),
            ReadBody());
}

TEST_F(URLLoaderTest, ResumeReadingBodyFromNetAfterClosingConsumer) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContentsFirstHalf = "This is the first half.";

  net::EmbeddedTestServer server;
  ControllableHttpResponse response_controller(&server, kPath);
  ASSERT_TRUE(server.Start());

  network::ResourceRequest request =
      CreateResourceRequest("GET", server.GetURL(kPath));

  network::mojom::URLLoaderPtr loader;
  // The loader is implicitly owned by the client and the NetworkContext.
  new URLLoader(context(), mojo::MakeRequest(&loader), 0, request, false,
                client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS,
                0);

  loader->PauseReadingBodyFromNet();
  loader.FlushForTesting();

  response_controller.WaitForRequest();
  response_controller.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n\r\n" +
      std::string(kBodyContentsFirstHalf));

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  auto response_body = client()->response_body_release();
  response_body.reset();

  // It shouldn't cause any issue even if a ResumeReadingBodyFromNet() call is
  // made after the response body data pipe is closed.
  loader->ResumeReadingBodyFromNet();
  loader.FlushForTesting();
}

TEST_F(URLLoaderTest, MultiplePauseResumeReadingBodyFromNet) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContentsFirstHalf = "This is the first half.";
  const char* const kBodyContentsSecondHalf = "This is the second half.";

  net::EmbeddedTestServer server;
  ControllableHttpResponse response_controller(&server, kPath);
  ASSERT_TRUE(server.Start());

  network::ResourceRequest request =
      CreateResourceRequest("GET", server.GetURL(kPath));

  network::mojom::URLLoaderPtr loader;
  // The loader is implicitly owned by the client and the NetworkContext.
  new URLLoader(context(), mojo::MakeRequest(&loader), 0, request, false,
                client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS,
                0);

  // It is okay to call ResumeReadingBodyFromNet() even if there is no prior
  // PauseReadingBodyFromNet().
  loader->ResumeReadingBodyFromNet();
  loader.FlushForTesting();

  response_controller.WaitForRequest();
  response_controller.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n\r\n" +
      std::string(kBodyContentsFirstHalf));

  loader->PauseReadingBodyFromNet();

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  loader->PauseReadingBodyFromNet();
  loader->PauseReadingBodyFromNet();
  loader.FlushForTesting();

  response_controller.Send(kBodyContentsSecondHalf);
  response_controller.Done();

  // One ResumeReadingBodyFromNet() call will resume reading even if there are
  // multiple PauseReadingBodyFromNet() calls before it.
  loader->ResumeReadingBodyFromNet();

  client()->RunUntilComplete();

  EXPECT_EQ(std::string(kBodyContentsFirstHalf) +
                std::string(kBodyContentsSecondHalf),
            ReadBody());
}

TEST_F(URLLoaderTest, UploadBytes) {
  const std::string kRequestBody = "Request Body";

  scoped_refptr<network::ResourceRequestBody> request_body(
      new network::ResourceRequestBody());
  request_body->AppendBytes(kRequestBody.c_str(), kRequestBody.length());
  set_request_body(std::move(request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(kRequestBody, response_body);
}

TEST_F(URLLoaderTest, UploadFile) {
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  std::string expected_body;
  ASSERT_TRUE(base::ReadFileToString(file_path, &expected_body))
      << "File not found: " << file_path.value();

  scoped_refptr<network::ResourceRequestBody> request_body(
      new network::ResourceRequestBody());
  request_body->AppendFileRange(
      file_path, 0, std::numeric_limits<uint64_t>::max(), base::Time());
  set_request_body(std::move(request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(expected_body, response_body);
}

TEST_F(URLLoaderTest, UploadFileWithRange) {
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  std::string expected_body;
  ASSERT_TRUE(base::ReadFileToString(file_path, &expected_body))
      << "File not found: " << file_path.value();
  expected_body = expected_body.substr(1, expected_body.size() - 2);

  scoped_refptr<network::ResourceRequestBody> request_body(
      new network::ResourceRequestBody());
  request_body->AppendFileRange(file_path, 1, expected_body.size(),
                                base::Time());
  set_request_body(std::move(request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(expected_body, response_body);
}

TEST_F(URLLoaderTest, UploadRawFile) {
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  std::string expected_body;
  ASSERT_TRUE(base::ReadFileToString(file_path, &expected_body))
      << "File not found: " << file_path.value();

  scoped_refptr<network::ResourceRequestBody> request_body(
      new network::ResourceRequestBody());
  request_body->AppendRawFileRange(
      OpenFileForUpload(file_path), GetTestFilePath("should_be_ignored"), 0,
      std::numeric_limits<uint64_t>::max(), base::Time());
  set_request_body(std::move(request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(expected_body, response_body);
}

TEST_F(URLLoaderTest, UploadRawFileWithRange) {
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  std::string expected_body;
  ASSERT_TRUE(base::ReadFileToString(file_path, &expected_body))
      << "File not found: " << file_path.value();
  expected_body = expected_body.substr(1, expected_body.size() - 2);

  scoped_refptr<network::ResourceRequestBody> request_body(
      new network::ResourceRequestBody());
  request_body->AppendRawFileRange(OpenFileForUpload(file_path),
                                   GetTestFilePath("should_be_ignored"), 1,
                                   expected_body.size(), base::Time());
  set_request_body(std::move(request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(expected_body, response_body);
}

// Tests a request body with a data pipe element.
TEST_F(URLLoaderTest, UploadDataPipe) {
  const std::string kRequestBody = "Request Body";

  network::mojom::DataPipeGetterPtr data_pipe_getter_ptr;
  auto data_pipe_getter = std::make_unique<network::TestDataPipeGetter>(
      kRequestBody, mojo::MakeRequest(&data_pipe_getter_ptr));

  auto resource_request_body =
      base::MakeRefCounted<network::ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_ptr));
  set_request_body(std::move(resource_request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(kRequestBody, response_body);
}

// Same as above and tests that the body is sent after a 307 redirect.
TEST_F(URLLoaderTest, UploadDataPipe_Redirect307) {
  const std::string kRequestBody = "Request Body";

  network::mojom::DataPipeGetterPtr data_pipe_getter_ptr;
  auto data_pipe_getter = std::make_unique<network::TestDataPipeGetter>(
      kRequestBody, mojo::MakeRequest(&data_pipe_getter_ptr));

  auto resource_request_body =
      base::MakeRefCounted<network::ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_ptr));
  set_request_body(std::move(resource_request_body));
  set_expect_redirect();

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/redirect307-to-echo"),
                          &response_body));
  EXPECT_EQ(kRequestBody, response_body);
}

// Tests a large request body, which should result in multiple asynchronous
// reads.
TEST_F(URLLoaderTest, UploadDataPipeWithLotsOfData) {
  std::string request_body;
  request_body.reserve(5 * 1024 * 1024);
  // Using a repeating patter with a length that's prime is more likely to spot
  // out of order or repeated chunks of data.
  while (request_body.size() < 5 * 1024 * 1024)
    request_body.append("foppity");

  network::mojom::DataPipeGetterPtr data_pipe_getter_ptr;
  auto data_pipe_getter = std::make_unique<network::TestDataPipeGetter>(
      request_body, mojo::MakeRequest(&data_pipe_getter_ptr));

  auto resource_request_body =
      base::MakeRefCounted<network::ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_ptr));
  set_request_body(std::move(resource_request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(request_body, response_body);
}

TEST_F(URLLoaderTest, UploadDataPipeError) {
  const std::string kRequestBody = "Request Body";

  network::mojom::DataPipeGetterPtr data_pipe_getter_ptr;
  auto data_pipe_getter = std::make_unique<network::TestDataPipeGetter>(
      kRequestBody, mojo::MakeRequest(&data_pipe_getter_ptr));
  data_pipe_getter->set_start_error(net::ERR_ACCESS_DENIED);

  auto resource_request_body =
      base::MakeRefCounted<network::ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_ptr));
  set_request_body(std::move(resource_request_body));

  EXPECT_EQ(net::ERR_ACCESS_DENIED, Load(test_server()->GetURL("/echo")));
}

TEST_F(URLLoaderTest, UploadDataPipeClosedEarly) {
  const std::string kRequestBody = "Request Body";

  network::mojom::DataPipeGetterPtr data_pipe_getter_ptr;
  auto data_pipe_getter = std::make_unique<network::TestDataPipeGetter>(
      kRequestBody, mojo::MakeRequest(&data_pipe_getter_ptr));
  data_pipe_getter->set_pipe_closed_early(true);

  auto resource_request_body =
      base::MakeRefCounted<network::ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_ptr));
  set_request_body(std::move(resource_request_body));

  std::string response_body;
  EXPECT_EQ(net::ERR_FAILED, Load(test_server()->GetURL("/echo")));
}

TEST_F(URLLoaderTest, UploadDoubleRawFile) {
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  std::string expected_body;
  ASSERT_TRUE(base::ReadFileToString(file_path, &expected_body))
      << "File not found: " << file_path.value();

  scoped_refptr<network::ResourceRequestBody> request_body(
      new network::ResourceRequestBody());
  request_body->AppendRawFileRange(
      OpenFileForUpload(file_path), GetTestFilePath("should_be_ignored"), 0,
      std::numeric_limits<uint64_t>::max(), base::Time());
  request_body->AppendRawFileRange(
      OpenFileForUpload(file_path), GetTestFilePath("should_be_ignored"), 0,
      std::numeric_limits<uint64_t>::max(), base::Time());
  set_request_body(std::move(request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(expected_body + expected_body, response_body);
}

// Tests that SSLInfo is not attached to OnComplete messages when there is no
// certificate error.
TEST_F(URLLoaderTest, NoSSLInfoWithoutCertificateError) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server.Start());
  set_send_ssl_for_cert_error();
  EXPECT_EQ(net::OK, Load(https_server.GetURL("/")));
  EXPECT_FALSE(client()->completion_status().ssl_info.has_value());
}

// Tests that SSLInfo is not attached to OnComplete messages when the
// corresponding option is not set.
TEST_F(URLLoaderTest, NoSSLInfoOnComplete) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server.Start());
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, Load(https_server.GetURL("/")));
  EXPECT_FALSE(client()->completion_status().ssl_info.has_value());
}

// Tests that SSLInfo is attached to OnComplete messages when the corresponding
// option is set.
TEST_F(URLLoaderTest, SSLInfoOnComplete) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server.Start());
  set_send_ssl_for_cert_error();
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, Load(https_server.GetURL("/")));
  ASSERT_TRUE(client()->completion_status().ssl_info.has_value());
  EXPECT_TRUE(client()->completion_status().ssl_info.value().cert);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            client()->completion_status().ssl_info.value().cert_status);
}

// A mock URLRequestJob which simulates an HTTPS request with a certificate
// error.
class MockHTTPSURLRequestJob : public net::URLRequestTestJob {
 public:
  MockHTTPSURLRequestJob(net::URLRequest* request,
                         net::NetworkDelegate* network_delegate,
                         const std::string& response_headers,
                         const std::string& response_data,
                         bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance) {}

  // net::URLRequestTestJob:
  void GetResponseInfo(net::HttpResponseInfo* info) override {
    // Get the original response info, but override the SSL info.
    net::URLRequestJob::GetResponseInfo(info);
    info->ssl_info.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    info->ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  }

 private:
  ~MockHTTPSURLRequestJob() override {}

  DISALLOW_COPY_AND_ASSIGN(MockHTTPSURLRequestJob);
};

class MockHTTPSJobURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  MockHTTPSJobURLRequestInterceptor() {}
  ~MockHTTPSJobURLRequestInterceptor() override {}

  // net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new MockHTTPSURLRequestJob(request, network_delegate, std::string(),
                                      "dummy response", true);
  }
};

// Tests that |cert_status| is set on the resource response.
TEST_F(URLLoaderTest, CertStatusOnResponse) {
  net::URLRequestFilter::GetInstance()->ClearHandlers();
  net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
      "https", "example.test",
      std::unique_ptr<net::URLRequestInterceptor>(
          new MockHTTPSJobURLRequestInterceptor()));

  EXPECT_EQ(net::OK, Load(GURL("https://example.test/")));
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            client()->response_head().cert_status);
}

}  // namespace content
