// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/network/network_context.h"
#include "content/network/url_loader_impl.h"
#include "content/public/common/appcache_info.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/controllable_http_response.h"
#include "content/public/test/test_url_loader_client.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

static ResourceRequest CreateResourceRequest(const char* method,
                                             ResourceType type,
                                             const GURL& url) {
  ResourceRequest request;
  request.method = std::string(method);
  request.url = url;
  request.site_for_cookies = url;  // bypass third-party cookie blocking
  request.request_initiator =
      url::Origin::Create(url);  // ensure initiator is set
  request.referrer_policy = blink::kWebReferrerPolicyDefault;
  request.load_flags = 0;
  request.origin_pid = 0;
  request.resource_type = type;
  request.request_context = 0;
  request.appcache_host_id = kAppCacheNoHostId;
  request.download_to_file = false;
  request.should_reset_appcache = false;
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

}  // namespace

class URLLoaderImplTest : public testing::Test {
 public:
  URLLoaderImplTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        context_(NetworkContext::CreateForTesting()) {
    net::URLRequestFailedJob::AddUrlHandler();
  }
  ~URLLoaderImplTest() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  void SetUp() override {
    test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    ASSERT_TRUE(test_server_.Start());
  }

  // Attempts to load |url| and returns the resulting error code. If |body| is
  // non-NULL, also attempts to read the response body. The advantage of using
  // |body| instead of calling ReadBody() after Load is that it will load the
  // response body before URLLoader is complete, so URLLoader completion won't
  // block on trying to write the body buffer.
  int Load(const GURL& url, std::string* body = nullptr) WARN_UNUSED_RESULT {
    DCHECK(!ran_);
    mojom::URLLoaderPtr loader;

    ResourceRequest request =
        CreateResourceRequest("GET", RESOURCE_TYPE_MAIN_FRAME, url);
    uint32_t options = mojom::kURLLoadOptionNone;
    if (send_ssl_)
      options |= mojom::kURLLoadOptionSendSSLInfo;
    if (sniff_)
      options |= mojom::kURLLoadOptionSniffMimeType;

    URLLoaderImpl loader_impl(context(), mojo::MakeRequest(&loader), options,
                              request, false, client_.CreateInterfacePtr(),
                              TRAFFIC_ANNOTATION_FOR_TESTS);

    ran_ = true;

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
    base::FilePath file;
    PathService::Get(content::DIR_TEST_DATA, &file);
    file = file.AppendASCII(path);

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

  net::EmbeddedTestServer* test_server() { return &test_server_; }
  NetworkContext* context() { return context_.get(); }
  TestURLLoaderClient* client() { return &client_; }
  void DestroyContext() { context_.reset(); }

  // Configure how Load() works.
  void set_sniff() {
    DCHECK(!ran_);
    sniff_ = true;
  }
  void set_send_ssl() {
    DCHECK(!ran_);
    send_ssl_ = true;
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

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<NetworkContext> context_;
  bool sniff_ = false;
  bool send_ssl_ = false;
  // Used to ensure that methods are called either before or after a request is
  // made, since the test fixture is meant to be used only once.
  bool ran_ = false;
  TestURLLoaderClient client_;
};

TEST_F(URLLoaderImplTest, Basic) {
  LoadAndCompareFile("simple_page.html");
}

TEST_F(URLLoaderImplTest, Empty) {
  LoadAndCompareFile("empty.html");
}

TEST_F(URLLoaderImplTest, BasicSSL) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  GURL url = https_server.GetURL("/simple_page.html");
  set_send_ssl();
  EXPECT_EQ(net::OK, Load(url));
  ASSERT_TRUE(!!ssl_info());
  ASSERT_TRUE(!!ssl_info()->cert);

  ASSERT_TRUE(https_server.GetCertificate()->Equals(ssl_info()->cert.get()));
}

TEST_F(URLLoaderImplTest, SSLSentOnlyWhenRequested) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  GURL url = https_server.GetURL("/simple_page.html");
  EXPECT_EQ(net::OK, Load(url));
  ASSERT_FALSE(!!ssl_info());
}

// Test decoded_body_length / encoded_body_length when they're different.
TEST_F(URLLoaderImplTest, GzipTest) {
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

TEST_F(URLLoaderImplTest, ErrorBeforeHeaders) {
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE,
            Load(test_server()->GetURL("/close-socket"), nullptr));
  EXPECT_FALSE(client()->response_body().is_valid());
}

TEST_F(URLLoaderImplTest, SyncErrorWhileReadingBody) {
  std::string body;
  EXPECT_EQ(net::ERR_FAILED,
            Load(net::URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
                     net::URLRequestFailedJob::READ_SYNC, net::ERR_FAILED),
                 &body));
  EXPECT_EQ("", body);
}

TEST_F(URLLoaderImplTest, AsyncErrorWhileReadingBody) {
  std::string body;
  EXPECT_EQ(net::ERR_FAILED,
            Load(net::URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
                     net::URLRequestFailedJob::READ_ASYNC, net::ERR_FAILED),
                 &body));
  EXPECT_EQ("", body);
}

TEST_F(URLLoaderImplTest, SyncErrorWhileReadingBodyAfterBytesReceived) {
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

TEST_F(URLLoaderImplTest, AsyncErrorWhileReadingBodyAfterBytesReceived) {
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

TEST_F(URLLoaderImplTest, DestroyContextWithLiveRequest) {
  GURL url = test_server()->GetURL("/hung-after-headers");
  ResourceRequest request =
      CreateResourceRequest("GET", RESOURCE_TYPE_MAIN_FRAME, url);

  mojom::URLLoaderPtr loader;
  // The loader is implicitly owned by the client and the NetworkContext, so
  // don't hold on to a pointer to it.
  base::WeakPtr<URLLoaderImpl> loader_impl =
      (new URLLoaderImpl(context(), mojo::MakeRequest(&loader), 0, request,
                         false, client()->CreateInterfacePtr(),
                         TRAFFIC_ANNOTATION_FOR_TESTS))
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

TEST_F(URLLoaderImplTest, DoNotSniffUnlessSpecified) {
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test0.html")));
  ASSERT_TRUE(mime_type().empty());
}

TEST_F(URLLoaderImplTest, SniffMimeType) {
  set_sniff();
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test0.html")));
  ASSERT_EQ(std::string("text/html"), mime_type());
}

TEST_F(URLLoaderImplTest, RespectNoSniff) {
  set_sniff();
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/nosniff-test.html")));
  ASSERT_TRUE(mime_type().empty());
}

TEST_F(URLLoaderImplTest, DoNotSniffHTMLFromTextPlain) {
  set_sniff();
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test1.html")));
  ASSERT_EQ(std::string("text/plain"), mime_type());
}

TEST_F(URLLoaderImplTest, DoNotSniffHTMLFromImageGIF) {
  set_sniff();
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test2.html")));
  ASSERT_EQ(std::string("image/gif"), mime_type());
}

TEST_F(URLLoaderImplTest, EmptyHtmlIsTextPlain) {
  set_sniff();
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test4.html")));
  ASSERT_EQ(std::string("text/plain"), mime_type());
}

TEST_F(URLLoaderImplTest, EmptyHtmlIsTextPlainWithAsyncResponse) {
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
TEST_F(URLLoaderImplTest, FirstReadNotEnoughToSniff1) {
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
TEST_F(URLLoaderImplTest, FirstReadNotEnoughToSniff2) {
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
TEST_F(URLLoaderImplTest, LoneReadNotEnoughToSniff) {
  set_sniff();
  std::string first(net::kMaxBytesToSniff - 100, 'a');
  LoadPacketsAndVerifyContents(first, std::string());
  ASSERT_EQ(std::string("text/plain"), mime_type());
}

// Tests the simple case where the first read is enough to sniff.
TEST_F(URLLoaderImplTest, FirstReadIsEnoughToSniff) {
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

TEST_F(URLLoaderImplTest, CloseResponseBodyConsumerBeforeProducer) {
  net::EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::Bind([](const net::test_server::HttpRequest& request) {
        std::unique_ptr<net::test_server::HttpResponse> response =
            std::make_unique<NeverFinishedBodyHttpResponse>();
        return response;
      }));
  ASSERT_TRUE(server.Start());

  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_MAIN_FRAME, server.GetURL("/hello.html"));

  mojom::URLLoaderPtr loader;
  // The loader is implicitly owned by the client and the NetworkContext.
  new URLLoaderImpl(context(), mojo::MakeRequest(&loader), 0, request, false,
                    client()->CreateInterfacePtr(),
                    TRAFFIC_ANNOTATION_FOR_TESTS);

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

TEST_F(URLLoaderImplTest, PauseReadingBodyFromNetBeforeRespnoseHeaders) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContents = "This is the data as you requested.";

  net::EmbeddedTestServer server;
  ControllableHttpResponse response_controller(&server, kPath);
  ASSERT_TRUE(server.Start());

  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_MAIN_FRAME, server.GetURL(kPath));

  mojom::URLLoaderPtr loader;
  // The loader is implicitly owned by the client and the NetworkContext.
  new URLLoaderImpl(context(), mojo::MakeRequest(&loader), 0, request, false,
                    client()->CreateInterfacePtr(),
                    TRAFFIC_ANNOTATION_FOR_TESTS);

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

TEST_F(URLLoaderImplTest, PauseReadingBodyFromNetWhenReadIsPending) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContentsFirstHalf = "This is the first half.";
  const char* const kBodyContentsSecondHalf = "This is the second half.";

  net::EmbeddedTestServer server;
  ControllableHttpResponse response_controller(&server, kPath);
  ASSERT_TRUE(server.Start());

  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_MAIN_FRAME, server.GetURL(kPath));

  mojom::URLLoaderPtr loader;
  // The loader is implicitly owned by the client and the NetworkContext.
  new URLLoaderImpl(context(), mojo::MakeRequest(&loader), 0, request, false,
                    client()->CreateInterfacePtr(),
                    TRAFFIC_ANNOTATION_FOR_TESTS);

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

TEST_F(URLLoaderImplTest, ResumeReadingBodyFromNetAfterClosingConsumer) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContentsFirstHalf = "This is the first half.";

  net::EmbeddedTestServer server;
  ControllableHttpResponse response_controller(&server, kPath);
  ASSERT_TRUE(server.Start());

  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_MAIN_FRAME, server.GetURL(kPath));

  mojom::URLLoaderPtr loader;
  // The loader is implicitly owned by the client and the NetworkContext.
  new URLLoaderImpl(context(), mojo::MakeRequest(&loader), 0, request, false,
                    client()->CreateInterfacePtr(),
                    TRAFFIC_ANNOTATION_FOR_TESTS);

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

TEST_F(URLLoaderImplTest, MultiplePauseResumeReadingBodyFromNet) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContentsFirstHalf = "This is the first half.";
  const char* const kBodyContentsSecondHalf = "This is the second half.";

  net::EmbeddedTestServer server;
  ControllableHttpResponse response_controller(&server, kPath);
  ASSERT_TRUE(server.Start());

  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_MAIN_FRAME, server.GetURL(kPath));

  mojom::URLLoaderPtr loader;
  // The loader is implicitly owned by the client and the NetworkContext.
  new URLLoaderImpl(context(), mojo::MakeRequest(&loader), 0, request, false,
                    client()->CreateInterfacePtr(),
                    TRAFFIC_ANNOTATION_FOR_TESTS);

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

}  // namespace content
