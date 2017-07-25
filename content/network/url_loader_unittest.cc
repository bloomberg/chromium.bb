// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

static ResourceRequest CreateResourceRequest(const char* method,
                                             ResourceType type,
                                             const GURL& url) {
  ResourceRequest request;
  request.method = std::string(method);
  request.url = url;
  request.first_party_for_cookies = url;  // bypass third-party cookie blocking
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

std::string ReadData(MojoHandle consumer, size_t size) {
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
                        MOJO_WRITE_DATA_FLAG_ALL_OR_NONE),
           MOJO_RESULT_OK);
  CHECK_EQ(num_bytes, static_cast<uint32_t>(size));

  return std::string(buffer.data(), buffer.size());
}

}  // namespace

class URLLoaderImplTest : public testing::Test {
 public:
  URLLoaderImplTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        context_(NetworkContext::CreateForTesting()) {}
  ~URLLoaderImplTest() override {}

  void SetUp() override {
    test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    ASSERT_TRUE(test_server_.Start());
  }

  void Load(const GURL& url,
            TestURLLoaderClient* client,
            uint32_t options = 0) {
    mojom::URLLoaderPtr loader;

    ResourceRequest request =
        CreateResourceRequest("GET", RESOURCE_TYPE_MAIN_FRAME, url);

    URLLoaderImpl loader_impl(context(), mojo::MakeRequest(&loader), options,
                              request, client->CreateInterfacePtr(),
                              TRAFFIC_ANNOTATION_FOR_TESTS);

    client->RunUntilComplete();
  }

  void LoadAndCompareFile(const std::string& path) {
    TestURLLoaderClient client;
    GURL url = test_server()->GetURL(std::string("/") + path);
    Load(url, &client);

    base::FilePath file;
    PathService::Get(content::DIR_TEST_DATA, &file);
    file = file.AppendASCII(path);

    std::string file_contents;
    if (!base::ReadFileToString(file, &file_contents)) {
      ADD_FAILURE() << "File not found: " << file.value();
      return;
    }

    std::string data =
        ReadData(client.response_body().value(), file_contents.size());
    CHECK_EQ(data, file_contents);
  }

  std::string LoadAndReturnMimeType(bool sniff, const std::string& path) {
    TestURLLoaderClient client;
    GURL url = test_server()->GetURL(path);
    int32_t options =
        sniff ? mojom::kURLLoadOptionSniffMimeType : mojom::kURLLoadOptionNone;
    Load(url, &client, options);
    return client.response_head().mime_type;
  }

  net::EmbeddedTestServer* test_server() { return &test_server_; }
  NetworkContext* context() { return context_.get(); }
  void DestroyContext() { context_.reset(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<NetworkContext> context_;
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

  TestURLLoaderClient client;
  GURL url = https_server.GetURL("/simple_page.html");
  Load(url, &client, mojom::kURLLoadOptionSendSSLInfo);
  ASSERT_TRUE(!!client.ssl_info());
  ASSERT_TRUE(!!client.ssl_info()->cert);

  ASSERT_TRUE(
      https_server.GetCertificate()->Equals(client.ssl_info()->cert.get()));
}

TEST_F(URLLoaderImplTest, SSLSentOnlyWhenRequested) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  TestURLLoaderClient client;
  GURL url = https_server.GetURL("/simple_page.html");
  Load(url, &client, 0);
  ASSERT_FALSE(!!client.ssl_info());
}

TEST_F(URLLoaderImplTest, DestroyContextWithLiveRequest) {
  TestURLLoaderClient client;
  GURL url = test_server()->GetURL("/hung-after-headers");
  ResourceRequest request =
      CreateResourceRequest("GET", RESOURCE_TYPE_MAIN_FRAME, url);

  mojom::URLLoaderPtr loader;
  // The loader is implicitly owned by the client and the NetworkContext, so
  // don't hold on to a pointer to it.
  base::WeakPtr<URLLoaderImpl> loader_impl =
      (new URLLoaderImpl(context(), mojo::MakeRequest(&loader), 0, request,
                         client.CreateInterfacePtr(),
                         TRAFFIC_ANNOTATION_FOR_TESTS))
          ->GetWeakPtrForTests();

  client.RunUntilResponseReceived();
  EXPECT_TRUE(client.has_received_response());
  EXPECT_FALSE(client.has_received_completion());

  // Request hasn't completed, so the loader should not have been destroyed.
  EXPECT_TRUE(loader_impl);

  // Destroying the context should result in destroying the loader and the
  // client receiving a connection error.
  DestroyContext();
  EXPECT_FALSE(loader_impl);

  client.RunUntilConnectionError();
  EXPECT_FALSE(client.has_received_completion());
  EXPECT_EQ(0u, client.download_data_length());
}

TEST_F(URLLoaderImplTest, DoNotSniffUnlessSpecified) {
  std::string mime_type =
      LoadAndReturnMimeType(false, "/content-sniffer-test0.html");
  ASSERT_TRUE(mime_type.empty());
}

TEST_F(URLLoaderImplTest, SniffMimeType) {
  std::string mime_type =
      LoadAndReturnMimeType(true, "/content-sniffer-test0.html");
  ASSERT_EQ(std::string("text/html"), mime_type);
}

TEST_F(URLLoaderImplTest, RespectNoSniff) {
  std::string mime_type = LoadAndReturnMimeType(true, "/nosniff-test.html");
  ASSERT_TRUE(mime_type.empty());
}

TEST_F(URLLoaderImplTest, DoNotSniffHTMLFromTextPlain) {
  std::string mime_type =
      LoadAndReturnMimeType(true, "/content-sniffer-test1.html");
  ASSERT_EQ(std::string("text/plain"), mime_type);
}

TEST_F(URLLoaderImplTest, DoNotSniffHTMLFromImageGIF) {
  std::string mime_type =
      LoadAndReturnMimeType(true, "/content-sniffer-test2.html");
  ASSERT_EQ(std::string("image/gif"), mime_type);
}

TEST_F(URLLoaderImplTest, CantSniffEmptyHtml) {
  std::string mime_type =
      LoadAndReturnMimeType(true, "/content-sniffer-test4.html");
  ASSERT_TRUE(mime_type.empty());
}

}  // namespace content
