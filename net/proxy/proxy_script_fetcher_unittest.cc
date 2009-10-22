// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_script_fetcher.h"

#include "base/file_path.h"
#include "base/compiler_specific.h"
#include "base/path_service.h"
#include "net/base/net_util.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// TODO(eroman):
//   - Test canceling an outstanding request.
//   - Test deleting ProxyScriptFetcher while a request is in progress.

const wchar_t kDocRoot[] = L"net/data/proxy_script_fetcher_unittest";

struct FetchResult {
  int code;
  std::string bytes;
};

// A non-mock URL request which can access http:// and file:// urls.
class RequestContext : public URLRequestContext {
 public:
  RequestContext() {
    net::ProxyConfig no_proxy;
    host_resolver_ = net::CreateSystemHostResolver();
    proxy_service_ = net::ProxyService::CreateFixed(no_proxy);
    ssl_config_service_ = new net::SSLConfigServiceDefaults;

    http_transaction_factory_ =
        new net::HttpCache(net::HttpNetworkLayer::CreateFactory(
            host_resolver_, proxy_service_, ssl_config_service_),
            disk_cache::CreateInMemoryCacheBackend(0));
  }
  ~RequestContext() {
    delete http_transaction_factory_;
  }
};

// Required to be in net namespace by FRIEND_TEST.
namespace net {

// Get a file:// url relative to net/data/proxy/proxy_script_fetcher_unittest.
GURL GetTestFileUrl(const std::string& relpath) {
  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("net");
  path = path.AppendASCII("data");
  path = path.AppendASCII("proxy_script_fetcher_unittest");
  GURL base_url = FilePathToFileURL(path);
  return GURL(base_url.spec() + "/" + relpath);
}

typedef PlatformTest ProxyScriptFetcherTest;

TEST_F(ProxyScriptFetcherTest, FileUrl) {
  scoped_refptr<URLRequestContext> context = new RequestContext;
  scoped_ptr<ProxyScriptFetcher> pac_fetcher(
      ProxyScriptFetcher::Create(context));

  { // Fetch a non-existent file.
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(GetTestFileUrl("does-not-exist"),
                                    &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(ERR_FILE_NOT_FOUND, callback.WaitForResult());
    EXPECT_TRUE(bytes.empty());
  }
  { // Fetch a file that exists.
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(GetTestFileUrl("pac.txt"),
                                    &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(OK, callback.WaitForResult());
    EXPECT_EQ("-pac.txt-\n", bytes);
  }
}

// Note that all mime types are allowed for PAC file, to be consistent
// with other browsers.
TEST_F(ProxyScriptFetcherTest, HttpMimeType) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<URLRequestContext> context = new RequestContext;
  scoped_ptr<ProxyScriptFetcher> pac_fetcher(
      ProxyScriptFetcher::Create(context));

  { // Fetch a PAC with mime type "text/plain"
    GURL url = server->TestServerPage("files/pac.txt");
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(OK, callback.WaitForResult());
    EXPECT_EQ("-pac.txt-\n", bytes);
  }
  { // Fetch a PAC with mime type "text/html"
    GURL url = server->TestServerPage("files/pac.html");
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(OK, callback.WaitForResult());
    EXPECT_EQ("-pac.html-\n", bytes);
  }
  { // Fetch a PAC with mime type "application/x-ns-proxy-autoconfig"
    GURL url = server->TestServerPage("files/pac.nsproxy");
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(OK, callback.WaitForResult());
    EXPECT_EQ("-pac.nsproxy-\n", bytes);
  }
}

TEST_F(ProxyScriptFetcherTest, HttpStatusCode) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<URLRequestContext> context = new RequestContext;
  scoped_ptr<ProxyScriptFetcher> pac_fetcher(
      ProxyScriptFetcher::Create(context));

  { // Fetch a PAC which gives a 500 -- FAIL
    GURL url = server->TestServerPage("files/500.pac");
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(ERR_PAC_STATUS_NOT_OK, callback.WaitForResult());
    EXPECT_TRUE(bytes.empty());
  }
  { // Fetch a PAC which gives a 404 -- FAIL
    GURL url = server->TestServerPage("files/404.pac");
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(ERR_PAC_STATUS_NOT_OK, callback.WaitForResult());
    EXPECT_TRUE(bytes.empty());
  }
}

TEST_F(ProxyScriptFetcherTest, ContentDisposition) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<URLRequestContext> context = new RequestContext;
  scoped_ptr<ProxyScriptFetcher> pac_fetcher(
      ProxyScriptFetcher::Create(context));

  // Fetch PAC scripts via HTTP with a Content-Disposition header -- should
  // have no effect.
  GURL url = server->TestServerPage("files/downloadable.pac");
  std::string bytes;
  TestCompletionCallback callback;
  int result = pac_fetcher->Fetch(url, &bytes, &callback);
  EXPECT_EQ(ERR_IO_PENDING, result);
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_EQ("-downloadable.pac-\n", bytes);
}

TEST_F(ProxyScriptFetcherTest, NoCache) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<URLRequestContext> context = new RequestContext;
  scoped_ptr<ProxyScriptFetcher> pac_fetcher(
      ProxyScriptFetcher::Create(context));

  // Fetch a PAC script whose HTTP headers make it cacheable for 1 hour.
  GURL url = server->TestServerPage("files/cacheable_1hr.pac");
  {
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(OK, callback.WaitForResult());
    EXPECT_EQ("-cacheable_1hr.pac-\n", bytes);
  }

  // Now kill the HTTP server.
  EXPECT_TRUE(server->Stop());  // Verify it shutdown synchronously.
  server = NULL;

  // Try to fetch the file again -- if should fail, since the server is not
  // running anymore. (If it were instead being loaded from cache, we would
  // get a success.
  {
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(ERR_CONNECTION_REFUSED, callback.WaitForResult());
  }
}

TEST_F(ProxyScriptFetcherTest, TooLarge) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<URLRequestContext> context = new RequestContext;
  scoped_ptr<ProxyScriptFetcher> pac_fetcher(
      ProxyScriptFetcher::Create(context));

  // Set the maximum response size to 50 bytes.
  int prev_size = ProxyScriptFetcher::SetSizeConstraintForUnittest(50);

  // These two URLs are the same file, but are http:// vs file://
  GURL urls[] = {
    server->TestServerPage("files/large-pac.nsproxy"),
    GetTestFileUrl("large-pac.nsproxy")
  };

  // Try fetching URLs that are 101 bytes large. We should abort the request
  // after 50 bytes have been read, and fail with a too large error.
  for (size_t i = 0; i < arraysize(urls); ++i) {
    const GURL& url = urls[i];
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(ERR_FILE_TOO_BIG, callback.WaitForResult());
    EXPECT_TRUE(bytes.empty());
  }

  // Restore the original size bound.
  ProxyScriptFetcher::SetSizeConstraintForUnittest(prev_size);

  { // Make sure we can still fetch regular URLs.
    GURL url = server->TestServerPage("files/pac.nsproxy");
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(OK, callback.WaitForResult());
    EXPECT_EQ("-pac.nsproxy-\n", bytes);
  }
}

TEST_F(ProxyScriptFetcherTest, Hang) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<URLRequestContext> context = new RequestContext;
  scoped_ptr<ProxyScriptFetcher> pac_fetcher(
      ProxyScriptFetcher::Create(context));

  // Set the timeout period to 0.5 seconds.
  int prev_timeout =
      ProxyScriptFetcher::SetTimeoutConstraintForUnittest(500);

  // Try fetching a URL which takes 1.2 seconds. We should abort the request
  // after 500 ms, and fail with a timeout error.
  { GURL url = server->TestServerPage("slow/proxy.pac?1.2");
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(ERR_TIMED_OUT, callback.WaitForResult());
    EXPECT_TRUE(bytes.empty());
  }

  // Restore the original timeout period.
  ProxyScriptFetcher::SetTimeoutConstraintForUnittest(prev_timeout);

  { // Make sure we can still fetch regular URLs.
    GURL url = server->TestServerPage("files/pac.nsproxy");
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(OK, callback.WaitForResult());
    EXPECT_EQ("-pac.nsproxy-\n", bytes);
  }
}

// The ProxyScriptFetcher should decode any content-codings
// (like gzip, bzip, etc.), and apply any charset conversions to yield
// UTF8.
TEST_F(ProxyScriptFetcherTest, Encodings) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<URLRequestContext> context = new RequestContext;
  scoped_ptr<ProxyScriptFetcher> pac_fetcher(
      ProxyScriptFetcher::Create(context));

  // Test a response that is gzip-encoded -- should get inflated.
  {
    GURL url = server->TestServerPage("files/gzipped_pac");
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(OK, callback.WaitForResult());
    EXPECT_EQ("This data was gzipped.\n", bytes);
  }

  // Test a response that was served as UTF-16 (BE). It should
  // be converted to UTF8.
  {
    GURL url = server->TestServerPage("files/utf16be_pac");
    std::string bytes;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &bytes, &callback);
    EXPECT_EQ(ERR_IO_PENDING, result);
    EXPECT_EQ(OK, callback.WaitForResult());
    EXPECT_EQ("This was encoded as UTF-16BE.\n", bytes);
  }
}

}  // namespace net
