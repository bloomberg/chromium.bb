// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/network/network_context.h"
#include "content/network/url_loader_impl.h"
#include "content/public/common/appcache_info.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/test_url_loader_client.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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
  request.request_initiator = url::Origin(url);  // ensure initiator is set
  request.referrer_policy = blink::kWebReferrerPolicyDefault;
  request.load_flags = 0;
  request.origin_pid = 0;
  request.resource_type = type;
  request.request_context = 0;
  request.appcache_host_id = kAppCacheNoHostId;
  request.download_to_file = false;
  request.should_reset_appcache = false;
  request.is_main_frame = true;
  request.parent_is_main_frame = false;
  request.transition_type = ui::PAGE_TRANSITION_LINK;
  request.allow_download = true;
  return request;
}

class URLRequestMultipleWritesJob : public net::URLRequestJob {
 public:
  URLRequestMultipleWritesJob(net::URLRequest* request,
                              net::NetworkDelegate* network_delegate,
                              std::list<std::string> packets)
      : URLRequestJob(request, network_delegate),
        packets_(std::move(packets)),
        weak_factory_(this) {}

  // net::URLRequestJob implementation:
  void Start() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&URLRequestMultipleWritesJob::StartAsync,
                                  weak_factory_.GetWeakPtr()));
  }

  int ReadRawData(net::IOBuffer* buf, int buf_size) override {
    if (packets_.empty())
      return 0;

    std::string packet = packets_.front();
    packets_.pop_front();
    CHECK_GE(buf_size, static_cast<int>(packet.length()));
    memcpy(buf->data(), packet.c_str(), packet.length());
    return packet.length();
  }

 private:
  ~URLRequestMultipleWritesJob() override {}

  void StartAsync() { NotifyHeadersComplete(); }

  std::list<std::string> packets_;

  base::WeakPtrFactory<URLRequestMultipleWritesJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestMultipleWritesJob);
};

class MultipleWritesInterceptor : public net::URLRequestInterceptor {
 public:
  explicit MultipleWritesInterceptor(std::list<std::string> packets)
      : packets_(std::move(packets)) {}
  ~MultipleWritesInterceptor() override {}

  static GURL GetURL() { return GURL("http://foo"); }

  // URLRequestInterceptor implementation:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new URLRequestMultipleWritesJob(request, network_delegate,
                                           std::move(packets_));
  }

 private:
  std::list<std::string> packets_;

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

  void Load(const GURL& url) {
    EXPECT_EQ(net::OK, LoadWithError(url, nullptr, 0));
  }

  // Attempts to load |url| and returns the resulting error code. If |data| is
  // non-NULL, also attempts to read a response body of |expected_body_size|.
  // The advantage of using |data| instead of calling ReadData() after
  // LoadWithError is that it will load the response body before the URLLoader
  // is destroyed, so pipes may still be open.
  //
  // TODO(mmenke): Come up with a better test fixture.
  int LoadWithError(const GURL& url,
                    std::string* data,
                    size_t expected_body_size) {
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
                              request, client_.CreateInterfacePtr(),
                              TRAFFIC_ANNOTATION_FOR_TESTS);

    client_.RunUntilComplete();
    DCHECK(!ran_);
    ran_ = true;

    if (data)
      *data = ReadData(expected_body_size);

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

    Load(test_server()->GetURL(std::string("/") + path));
    EXPECT_EQ(expected, ReadData(expected.size()));
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

  void LoadPackets(std::list<std::string> packets) {
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        MultipleWritesInterceptor::GetURL(),
        std::unique_ptr<net::URLRequestInterceptor>(
            new MultipleWritesInterceptor(std::move(packets))));
    Load(MultipleWritesInterceptor::GetURL());
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  // If |second| is empty, then it's ignored.
  void LoadPacketsAndVerifyContents(const std::string& first,
                                    const std::string& second) {
    EXPECT_FALSE(first.empty());
    std::list<std::string> packets;
    packets.push_back(first);
    if (!second.empty())
      packets.push_back(second);
    LoadPackets(std::move(packets));
    std::string expected = first + second;
    EXPECT_EQ(expected, ReadData(expected.size()));
    EXPECT_EQ(
        expected.size(),
        static_cast<size_t>(client()->completion_status().decoded_body_length));
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

  std::string ReadData(size_t size) {
    DCHECK(ran_);
    MojoHandle consumer = client()->response_body().value();
    MojoResult rv =
        mojo::Wait(mojo::Handle(consumer), MOJO_HANDLE_SIGNAL_READABLE);
    if (!size) {
      CHECK_EQ(rv, MOJO_RESULT_FAILED_PRECONDITION);
      return std::string();
    }
    CHECK_EQ(rv, MOJO_RESULT_OK);
    std::vector<char> buffer(size);
    uint32_t num_bytes = static_cast<uint32_t>(size);
    CHECK_EQ(MojoReadData(consumer, buffer.data(), &num_bytes,
                          MOJO_READ_DATA_FLAG_ALL_OR_NONE),
             MOJO_RESULT_OK);
    CHECK_EQ(num_bytes, static_cast<uint32_t>(size));

    // No more data should remain on the pipe.
    CHECK_EQ(MojoReadData(consumer, buffer.data(), &num_bytes,
                          MOJO_READ_DATA_FLAG_ALL_OR_NONE),
             MOJO_RESULT_FAILED_PRECONDITION);

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
  Load(url);
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
  Load(url);
  ASSERT_FALSE(!!ssl_info());
}

// Test decoded_body_length / encoded_body_length when they're different.
TEST_F(URLLoaderImplTest, GzipTest) {
  Load(test_server()->GetURL("/gzip-body?Body"));
  EXPECT_EQ("Body", ReadData(4));
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
            LoadWithError(test_server()->GetURL("/close-socket"), nullptr, 0));
  EXPECT_FALSE(client()->response_body().is_valid());
}

TEST_F(URLLoaderImplTest, SyncErrorWhileReadingBody) {
  std::string body;
  EXPECT_EQ(
      net::ERR_FAILED,
      LoadWithError(net::URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
                        net::URLRequestFailedJob::READ_SYNC, net::ERR_FAILED),
                    &body, 0));
  EXPECT_EQ("", body);
}

TEST_F(URLLoaderImplTest, AsyncErrorWhileReadingBody) {
  std::string body;
  EXPECT_EQ(
      net::ERR_FAILED,
      LoadWithError(net::URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
                        net::URLRequestFailedJob::READ_ASYNC, net::ERR_FAILED),
                    &body, 0));
  EXPECT_EQ("", body);
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
                         client()->CreateInterfacePtr(),
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
  Load(test_server()->GetURL("/content-sniffer-test0.html"));
  ASSERT_TRUE(mime_type().empty());
}

TEST_F(URLLoaderImplTest, SniffMimeType) {
  set_sniff();
  Load(test_server()->GetURL("/content-sniffer-test0.html"));
  ASSERT_EQ(std::string("text/html"), mime_type());
}

TEST_F(URLLoaderImplTest, RespectNoSniff) {
  set_sniff();
  Load(test_server()->GetURL("/nosniff-test.html"));
  ASSERT_TRUE(mime_type().empty());
}

TEST_F(URLLoaderImplTest, DoNotSniffHTMLFromTextPlain) {
  set_sniff();
  Load(test_server()->GetURL("/content-sniffer-test1.html"));
  ASSERT_EQ(std::string("text/plain"), mime_type());
}

TEST_F(URLLoaderImplTest, DoNotSniffHTMLFromImageGIF) {
  set_sniff();
  Load(test_server()->GetURL("/content-sniffer-test2.html"));
  ASSERT_EQ(std::string("image/gif"), mime_type());
}

TEST_F(URLLoaderImplTest, CantSniffEmptyHtml) {
  set_sniff();
  Load(test_server()->GetURL("/content-sniffer-test4.html"));
  ASSERT_TRUE(mime_type().empty());
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

}  // namespace content
