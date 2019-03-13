// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/loader/prefetch_url_loader_service.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_package/mock_signed_exchange_handler.h"
#include "content/browser/web_package/signed_exchange_loader.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"

namespace content {

struct PrefetchBrowserTestParam {
  PrefetchBrowserTestParam(bool signed_exchange_enabled,
                           bool network_service_enabled)
      : signed_exchange_enabled(signed_exchange_enabled),
        network_service_enabled(network_service_enabled) {}
  const bool signed_exchange_enabled;
  const bool network_service_enabled;
};

struct ScopedSignedExchangeHandlerFactory {
  explicit ScopedSignedExchangeHandlerFactory(
      SignedExchangeHandlerFactory* factory) {
    SignedExchangeLoader::SetSignedExchangeHandlerFactoryForTest(factory);
  }
  ~ScopedSignedExchangeHandlerFactory() {
    SignedExchangeLoader::SetSignedExchangeHandlerFactoryForTest(nullptr);
  }
};

class PrefetchBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<PrefetchBrowserTestParam> {
 public:
  struct ResponseEntry {
    ResponseEntry() = default;
    explicit ResponseEntry(
        const std::string& content,
        const std::string& content_type = "text/html",
        const std::vector<std::pair<std::string, std::string>>& headers = {})
        : content(content), content_type(content_type), headers(headers) {}
    ~ResponseEntry() = default;
    std::string content;
    std::string content_type;
    std::vector<std::pair<std::string, std::string>> headers;
  };

  PrefetchBrowserTest() {
    cross_origin_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }
  ~PrefetchBrowserTest() = default;

  void SetUp() override {
    std::vector<base::Feature> enable_features;
    std::vector<base::Feature> disabled_features;
    if (GetParam().signed_exchange_enabled) {
      enable_features.push_back(features::kSignedHTTPExchange);
    } else {
      disabled_features.push_back(features::kSignedHTTPExchange);
    }
    if (GetParam().network_service_enabled) {
      enable_features.push_back(network::features::kNetworkService);
    } else {
      disabled_features.push_back(network::features::kNetworkService);
    }
    feature_list_.InitWithFeatures(enable_features, disabled_features);
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext()));
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        BindOnce(
            &PrefetchURLLoaderService::RegisterPrefetchLoaderCallbackForTest,
            base::RetainedRef(partition->GetPrefetchURLLoaderService()),
            base::BindRepeating(&PrefetchBrowserTest::OnPrefetchURLLoaderCalled,
                                base::Unretained(this))));
  }

  void RegisterResponse(const std::string& url, const ResponseEntry& entry) {
    response_map_[url] = entry;
  }

  std::unique_ptr<net::test_server::HttpResponse> ServeResponses(
      const net::test_server::HttpRequest& request) {
    auto found = response_map_.find(request.relative_url);
    if (found != response_map_.end()) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content(found->second.content);
      response->set_content_type(found->second.content_type);
      for (const auto& header : found->second.headers) {
        response->AddCustomHeader(header.first, header.second);
      }
      return std::move(response);
    }
    return nullptr;
  }

  void WatchURLAndRunClosure(
      const std::string& relative_url,
      int* visit_count,
      base::OnceClosure closure,
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == relative_url) {
      (*visit_count)++;
      if (closure)
        std::move(closure).Run();
    }
  }

  void OnPrefetchURLLoaderCalled() { prefetch_url_loader_called_++; }

  void RegisterRequestMonitor(net::EmbeddedTestServer* test_server,
                              const std::string& path,
                              int* count,
                              base::RunLoop* waiter) {
    test_server->RegisterRequestMonitor(base::BindRepeating(
        &PrefetchBrowserTest::WatchURLAndRunClosure, base::Unretained(this),
        path, count,
        waiter ? waiter->QuitClosure() : base::RepeatingClosure()));
  }

  void RegisterRequestHandler(net::EmbeddedTestServer* test_server) {
    test_server->RegisterRequestHandler(base::BindRepeating(
        &PrefetchBrowserTest::ServeResponses, base::Unretained(this)));
  }

  void NavigateToURLAndWaitTitle(const GURL& url, const std::string& title) {
    base::string16 title16 = base::ASCIIToUTF16(title);
    TitleWatcher title_watcher(shell()->web_contents(), title16);
    // Execute the JavaScript code to triger the followup navigation from the
    // current page.
    EXPECT_TRUE(ExecuteScript(
        shell()->web_contents(),
        base::StringPrintf("location.href = '%s';", url.spec().c_str())));
    EXPECT_EQ(title16, title_watcher.WaitAndGetTitle());
  }

  void WaitUntilLoaded(const GURL& url) {
    int entry_count = 0;
    while (entry_count == 0) {
      const bool result = ExecuteScriptAndExtractInt(
          shell()->web_contents(),
          base::StringPrintf("window.domAutomationController.send("
                             "performance.getEntriesByName('%s').length);",
                             url.spec().c_str()),
          &entry_count);
      ASSERT_TRUE(result);
    }
  }

  int prefetch_url_loader_called_ = 0;
  std::unique_ptr<net::EmbeddedTestServer> cross_origin_server_;

 private:
  base::test::ScopedFeatureList feature_list_;
  std::map<std::string, ResponseEntry> response_map_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchBrowserTest);
};

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, Simple) {
  int target_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_path)));
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>"));

  base::RunLoop prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), target_path,
                         &target_fetch_count, &prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, target_fetch_count);
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_url = embedded_test_server()->GetURL(target_path);

  // Loading a page that prefetches the target URL would increment the
  // |target_fetch_count|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_fetch_count);
  EXPECT_EQ(1, prefetch_url_loader_called_);

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_url, "Prefetch Target");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, CrossOrigin) {
  int target_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>"));

  base::RunLoop prefetch_waiter;
  RegisterRequestMonitor(cross_origin_server_.get(), target_path,
                         &target_fetch_count, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL(target_path);
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s'></body>",
                       cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, target_fetch_count);
  EXPECT_EQ(0, prefetch_url_loader_called_);

  // Loading a page that prefetches the target URL would increment the
  // |target_fetch_count|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_fetch_count);
  EXPECT_EQ(1, prefetch_url_loader_called_);

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(cross_origin_target_url, "Prefetch Target");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, DoublePrefetch) {
  int target_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(prefetch_path, ResponseEntry(base::StringPrintf(
                                      "<body><link rel='prefetch' href='%s'>"
                                      "<link rel='prefetch' href='%s'></body>",
                                      target_path, target_path)));
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>"));

  base::RunLoop prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), target_path,
                         &target_fetch_count, &prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, target_fetch_count);
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_url = embedded_test_server()->GetURL(target_path);

  // Loading a page that prefetches the target URL would increment the
  // |target_fetch_count|, but it should hit only once.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_fetch_count);
  EXPECT_EQ(1, prefetch_url_loader_called_);

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_url, "Prefetch Target");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, NoCacheAndNoStore) {
  int nocache_fetch_count = 0;
  int nostore_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* nocache_path = "/target1.html";
  const char* nostore_path = "/target2.html";

  RegisterResponse(prefetch_path, ResponseEntry(base::StringPrintf(
                                      "<body>"
                                      "<link rel='prefetch' href='%s'>"
                                      "<link rel='prefetch' href='%s'></body>",
                                      nocache_path, nostore_path)));
  RegisterResponse(nocache_path,
                   ResponseEntry("<head><title>NoCache Target</title></head>",
                                 "text/html", {{"cache-control", "no-cache"}}));
  RegisterResponse(nostore_path,
                   ResponseEntry("<head><title>NoStore Target</title></head>",
                                 "text/html", {{"cache-control", "no-store"}}));

  base::RunLoop nocache_waiter;
  base::RunLoop nostore_waiter;
  RegisterRequestMonitor(embedded_test_server(), nocache_path,
                         &nocache_fetch_count, &nocache_waiter);
  RegisterRequestMonitor(embedded_test_server(), nostore_path,
                         &nostore_fetch_count, &nostore_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  // Loading a page that prefetches the target URL would increment the
  // fetch count for the both targets.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  nocache_waiter.Run();
  nostore_waiter.Run();
  EXPECT_EQ(1, nocache_fetch_count);
  EXPECT_EQ(1, nostore_fetch_count);
  EXPECT_EQ(2, prefetch_url_loader_called_);

  // Subsequent navigation to the no-cache URL wouldn't hit the network, because
  // no-cache resource is kept available up to kPrefetchReuseMins.
  NavigateToURLAndWaitTitle(embedded_test_server()->GetURL(nocache_path),
                            "NoCache Target");
  EXPECT_EQ(1, nocache_fetch_count);

  // Subsequent navigation to the no-store URL hit the network again, because
  // no-store resource is not cached even for prefetch.
  NavigateToURLAndWaitTitle(embedded_test_server()->GetURL(nostore_path),
                            "NoStore Target");
  EXPECT_EQ(2, nostore_fetch_count);

  EXPECT_EQ(2, prefetch_url_loader_called_);
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, WithPreload) {
  int target_fetch_count = 0;
  int preload_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  const char* preload_path = "/preload.js";
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_path)));
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "text/html",
                    {{"link", "</preload.js>;rel=\"preload\";as=\"script\""}}));
  RegisterResponse(preload_path,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  RegisterRequestMonitor(embedded_test_server(), target_path,
                         &target_fetch_count, nullptr /* waiter */);
  RegisterRequestMonitor(embedded_test_server(), preload_path,
                         &preload_fetch_count, &preload_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_url = embedded_test_server()->GetURL(target_path);

  // Loading a page that prefetches the target URL would increment both
  // |target_fetch_count| and |preload_fetch_count|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  preload_waiter.Run();
  EXPECT_EQ(1, target_fetch_count);
  EXPECT_EQ(1, preload_fetch_count);
  EXPECT_EQ(1, prefetch_url_loader_called_);

  WaitUntilLoaded(embedded_test_server()->GetURL(preload_path));

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  NavigateToURLAndWaitTitle(target_url, "done");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, CrossOriginWithPreload) {
  int target_fetch_count = 0;
  int preload_fetch_count = 0;
  const char* target_path = "/target.html";
  const char* preload_path = "/preload.js";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "text/html",
                    {{"link", "</preload.js>;rel=\"preload\";as=\"script\""},
                     {"access-control-allow-origin", "*"}}));
  RegisterResponse(preload_path,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;

  RegisterRequestMonitor(cross_origin_server_.get(), target_path,
                         &target_fetch_count, nullptr /* waiter */);
  RegisterRequestMonitor(cross_origin_server_.get(), preload_path,
                         &preload_fetch_count, &preload_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL(target_path);
  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path, ResponseEntry(base::StringPrintf(
                                      "<body><link rel='prefetch' href='%s' "
                                      "crossorigin=\"anonymous\"></body>",
                                      cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  // Loading a page that prefetches the target URL would increment both
  // |target_fetch_count| and |preload_fetch_count|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  preload_waiter.Run();
  EXPECT_EQ(1, target_fetch_count);
  EXPECT_EQ(1, preload_fetch_count);
  EXPECT_EQ(1, prefetch_url_loader_called_);

  WaitUntilLoaded(cross_origin_server_->GetURL(preload_path));

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(cross_origin_target_url, "done");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, WebPackageWithPreload) {
  int target_fetch_count = 0;
  int preload_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* preload_path_in_sxg = "/preload.js";

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML content
      // as "application/signed-exchange;v=b3".
      ResponseEntry("<head><title>Prefetch Target (SXG)</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"}}));
  RegisterResponse(preload_path_in_sxg,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  base::RunLoop prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), target_sxg_path,
                         &target_fetch_count, &prefetch_waiter);
  RegisterRequestMonitor(embedded_test_server(), preload_path_in_sxg,
                         &preload_fetch_count, &preload_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL preload_url_in_sxg =
      embedded_test_server()->GetURL(preload_path_in_sxg);
  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);

  MockSignedExchangeHandlerFactory factory(
      SignedExchangeLoadResult::kSuccess, net::OK,
      GURL(embedded_test_server()->GetURL(target_path)), "text/html",
      {base::StringPrintf("Link: <%s>;rel=\"preload\";as=\"script\"",
                          preload_url_in_sxg.spec().c_str())});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  // Loading a page that prefetches the target URL would increment both
  // |target_fetch_count| and |preload_fetch_count|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_fetch_count);

  // Test after this point requires SignedHTTPExchange support
  if (!signed_exchange_utils::IsSignedExchangeHandlingEnabled())
    return;

  // If the header in the .sxg file is correctly extracted, we should
  // be able to also see the preload.
  preload_waiter.Run();
  EXPECT_EQ(1, preload_fetch_count);
  EXPECT_EQ(1, prefetch_url_loader_called_);

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_sxg_url, "done");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, CrossOriginWebPackageWithPreload) {
  int target_fetch_count = 0;
  int preload_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* preload_path_in_sxg = "/preload.js";

  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML content
      // as "application/signed-exchange;v=b3".
      ResponseEntry("<head><title>Prefetch Target (SXG)</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"}}));
  RegisterResponse(preload_path_in_sxg,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  base::RunLoop prefetch_waiter;
  RegisterRequestMonitor(cross_origin_server_.get(), target_sxg_path,
                         &target_fetch_count, &prefetch_waiter);
  RegisterRequestMonitor(cross_origin_server_.get(), preload_path_in_sxg,
                         &preload_fetch_count, &preload_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL target_sxg_url = cross_origin_server_->GetURL(target_sxg_path);
  const GURL preload_url_in_sxg =
      cross_origin_server_->GetURL(preload_path_in_sxg);

  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s'></body>",
                       target_sxg_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  MockSignedExchangeHandlerFactory factory(
      SignedExchangeLoadResult::kSuccess, net::OK,
      GURL(cross_origin_server_->GetURL(target_path)), "text/html",
      {base::StringPrintf("Link: <%s>;rel=\"preload\";as=\"script\"",
                          preload_url_in_sxg.spec().c_str())});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  // Loading a page that prefetches the target URL would increment both
  // |target_fetch_count| and |preload_fetch_count|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_fetch_count);

  // Test after this point requires SignedHTTPExchange support
  if (!signed_exchange_utils::IsSignedExchangeHandlingEnabled())
    return;
  // If the header in the .sxg file is correctly extracted, we should
  // be able to also see the preload.
  preload_waiter.Run();
  EXPECT_EQ(1, preload_fetch_count);

  EXPECT_EQ(1, prefetch_url_loader_called_);

  WaitUntilLoaded(preload_url_in_sxg);

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_sxg_url, "done");
}

INSTANTIATE_TEST_SUITE_P(PrefetchBrowserTest,
                         PrefetchBrowserTest,
                         testing::Values(PrefetchBrowserTestParam(false, false),
                                         PrefetchBrowserTestParam(false, true),
                                         PrefetchBrowserTestParam(true, false),
                                         PrefetchBrowserTestParam(true, true)));

}  // namespace content
