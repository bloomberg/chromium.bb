// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/prefetch_browsertest_base.h"
#include "content/browser/loader/prefetched_signed_exchange_cache.h"
#include "content/browser/web_package/mock_signed_exchange_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/cross_origin_read_blocking.h"
#include "services/network/public/cpp/features.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

namespace {

std::string GetHeaderIntegrityString(const net::SHA256HashValue& hash) {
  std::string header_integrity_string = net::HashValue(hash).ToString();
  // Change "sha256/" to "sha256-".
  header_integrity_string[6] = '-';
  return header_integrity_string;
}

}  // namespace

struct SignedExchangeSubresourcePrefetchBrowserTestParam {
  SignedExchangeSubresourcePrefetchBrowserTestParam(
      bool network_service_enabled)
      : network_service_enabled(network_service_enabled) {}
  const bool network_service_enabled;
};

class SignedExchangeSubresourcePrefetchBrowserTest
    : public PrefetchBrowserTestBase,
      public testing::WithParamInterface<
          SignedExchangeSubresourcePrefetchBrowserTestParam> {
 public:
  SignedExchangeSubresourcePrefetchBrowserTest() = default;
  ~SignedExchangeSubresourcePrefetchBrowserTest() = default;

  void SetUp() override {
    std::vector<base::Feature> enable_features;
    std::vector<base::Feature> disabled_features;
    enable_features.push_back(features::kSignedHTTPExchange);
    enable_features.push_back(features::kSignedExchangeSubresourcePrefetch);
    if (GetParam().network_service_enabled) {
      enable_features.push_back(network::features::kNetworkService);
    } else {
      disabled_features.push_back(network::features::kNetworkService);
    }
    feature_list_.InitWithFeatures(enable_features, disabled_features);
    PrefetchBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    PrefetchBrowserTestBase::SetUpOnMainThread();
  }

 protected:
  static constexpr size_t kTestBlobStorageIPCThresholdBytes = 20;
  static constexpr size_t kTestBlobStorageMaxSharedMemoryBytes = 50;
  static constexpr size_t kTestBlobStorageMaxBlobMemorySize = 400;
  static constexpr uint64_t kTestBlobStorageMaxDiskSpace = 500;
  static constexpr uint64_t kTestBlobStorageMinFileSizeBytes = 10;
  static constexpr uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

  PrefetchedSignedExchangeCache::EntryMap GetCachedExchanges() {
    RenderViewHost* rvh = shell()->web_contents()->GetRenderViewHost();
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(rvh->GetMainFrame());
    scoped_refptr<PrefetchedSignedExchangeCache> cache =
        rfh->EnsurePrefetchedSignedExchangeCache();
    PrefetchedSignedExchangeCache::EntryMap results;
    base::RunLoop run_loop;
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            [](scoped_refptr<PrefetchedSignedExchangeCache> cache,
               PrefetchedSignedExchangeCache::EntryMap* results) {
              for (const auto& exchanges_it : cache->exchanges_) {
                (*results)[exchanges_it.first] = exchanges_it.second->Clone();
              }
            },
            cache, &results),
        run_loop.QuitClosure());
    run_loop.Run();
    return results;
  }

  void SetBlobLimits() {
    scoped_refptr<ChromeBlobStorageContext> blob_context =
        ChromeBlobStorageContext::GetFor(
            shell()->web_contents()->GetBrowserContext());
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &SignedExchangeSubresourcePrefetchBrowserTest::SetBlobLimitsOnIO,
            blob_context));
  }

  // Test that CORB logic works well with prefetched signed exchange
  // subresources. This test loads a main SXG which signed by
  // "publisher.example.com", and loads a SXG subresource using a <script> tag.
  //
  // When |cross_origin| is set, the SXG subresource is signed by
  // "crossorigin.example.com", otherwise sined by "publisher.example.com".
  // |content| is the inner body content of the SXG subresource.
  // |mime_type| is the MIME tyepe of the inner response of the SXG subresource.
  // When |has_nosniff| is set, the inner response header of the SXG subresource
  // has "x-content-type-options: nosniff" header.
  // |expected_title| is the expected title after loading the SXG page. If the
  // content load should not be blocked, this should be the title which is set
  // by executing |content|, otherwise this should be "original title".
  // |expected_action| is the expected CrossOriginReadBlocking::Action which is
  // recorded in the histogram.
  void RunCrossOriginReadBlockingTest(
      bool cross_origin,
      const std::string& content,
      const std::string& mime_type,
      bool has_nosniff,
      const std::string& expected_title,
      network::CrossOriginReadBlocking::Action expected_action) {
    int script_sxg_fetch_count = 0;
    int script_fetch_count = 0;
    const char* prefetch_path = "/prefetch.html";
    const char* target_sxg_path = "/target.sxg";
    const char* target_path = "/target.html";
    const char* script_sxg_path = "/script_js.sxg";
    const char* script_path = "/script.js";

    base::RunLoop script_sxg_prefetch_waiter;
    RegisterRequestMonitor(embedded_test_server(), script_sxg_path,
                           &script_sxg_fetch_count,
                           &script_sxg_prefetch_waiter);
    base::RunLoop script_prefetch_waiter;
    RegisterRequestMonitor(embedded_test_server(), script_path,
                           &script_fetch_count, &script_prefetch_waiter);
    RegisterRequestHandler(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
    const GURL target_url =
        embedded_test_server()->GetURL("publisher.example.com", target_path);
    const GURL script_sxg_url = embedded_test_server()->GetURL(script_sxg_path);
    const GURL script_url = embedded_test_server()->GetURL(
        cross_origin ? "crossorigin.example.com" : "publisher.example.com",
        script_path);

    const net::SHA256HashValue target_header_integrity = {{0x01}};
    const net::SHA256HashValue script_header_integrity = {{0x02}};
    const std::string script_header_integrity_string =
        GetHeaderIntegrityString(script_header_integrity);

    const std::string outer_link_header = base::StringPrintf(
        "<%s>;"
        "rel=\"alternate\";"
        "type=\"application/signed-exchange;v=b3\";"
        "anchor=\"%s\"",
        script_sxg_url.spec().c_str(), script_url.spec().c_str());
    const std::string inner_link_headers = base::StringPrintf(
        "Link: "
        "<%s>;rel=\"allowed-alt-sxg\";header-integrity=\"%s\","
        "<%s>;rel=\"preload\";as=\"script\"",
        script_url.spec().c_str(), script_header_integrity_string.c_str(),
        script_url.spec().c_str());

    RegisterResponse(
        prefetch_path,
        ResponseEntry(base::StringPrintf(
            "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
    RegisterResponse(
        script_path,
        ResponseEntry("document.title=\"from server\";", "text/javascript"));
    RegisterResponse(
        target_sxg_path,
        ResponseEntry(base::StringPrintf(
                          "<head><script src=\"%s\"></script><title>original "
                          "title</title>"
                          "</head>",
                          script_url.spec().c_str()),
                      "application/signed-exchange;v=b3",
                      {{"x-content-type-options", "nosniff"},
                       {"link", outer_link_header}}));
    RegisterResponse(script_sxg_path,
                     ResponseEntry(content, "application/signed-exchange;v=b3",
                                   {{"x-content-type-options", "nosniff"}}));
    std::vector<std::string> script_inner_response_headers;
    if (has_nosniff) {
      script_inner_response_headers.emplace_back(
          "x-content-type-options: nosniff");
    }
    MockSignedExchangeHandlerFactory factory(
        {MockSignedExchangeHandlerParams(
             target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
             target_url, "text/html", {inner_link_headers},
             target_header_integrity),
         MockSignedExchangeHandlerParams(
             script_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
             script_url, mime_type, script_inner_response_headers,
             script_header_integrity)});
    ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

    NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
    script_sxg_prefetch_waiter.Run();
    EXPECT_EQ(1, script_sxg_fetch_count);
    EXPECT_EQ(0, script_fetch_count);

    WaitUntilLoaded(target_sxg_url);
    WaitUntilLoaded(script_sxg_url);

    EXPECT_EQ(2u, GetCachedExchanges().size());

    {
      base::HistogramTester histograms;
      NavigateToURLAndWaitTitle(target_sxg_url, expected_title);
      histograms.ExpectBucketCount(
          "SiteIsolation.XSD.Browser.Action",
          network::CrossOriginReadBlocking::Action::kResponseStarted, 1);
      histograms.ExpectBucketCount("SiteIsolation.XSD.Browser.Action",
                                   expected_action, 1);
    }

    EXPECT_EQ(1, script_sxg_fetch_count);
    EXPECT_EQ(0, script_fetch_count);
  }

 private:
  static void SetBlobLimitsOnIO(
      scoped_refptr<ChromeBlobStorageContext> context) {
    storage::BlobStorageLimits limits;
    limits.max_ipc_memory_size = kTestBlobStorageIPCThresholdBytes;
    limits.max_shared_memory_size = kTestBlobStorageMaxSharedMemoryBytes;
    limits.max_blob_in_memory_space = kTestBlobStorageMaxBlobMemorySize;
    limits.desired_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.effective_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.min_page_file_size = kTestBlobStorageMinFileSizeBytes;
    limits.max_file_size = kTestBlobStorageMaxFileSizeBytes;
    context->context()->set_limits_for_testing(limits);
  }
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeSubresourcePrefetchBrowserTest);
};

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       PrefetchedSignedExchangeCache) {
  int target_sxg_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";

  base::RunLoop target_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), target_sxg_path,
                         &target_sxg_fetch_count, &target_sxg_prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML content
      // as "application/signed-exchange;v=b3".
      ResponseEntry("<head><title>Prefetch Target (SXG)</title></head>",
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"}}));

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK, target_url,
      "text/html", {}, target_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  target_sxg_prefetch_waiter.Run();
  EXPECT_EQ(1, target_sxg_fetch_count);

  WaitUntilLoaded(target_sxg_url);

  EXPECT_EQ(1, prefetch_url_loader_called_);

  const auto cached_exchanges = GetCachedExchanges();
  EXPECT_EQ(1u, cached_exchanges.size());
  const auto it = cached_exchanges.find(target_sxg_url);
  ASSERT_TRUE(it != cached_exchanges.end());
  EXPECT_EQ(target_sxg_url, it->second->outer_url());
  EXPECT_EQ(target_url, it->second->inner_url());
  EXPECT_EQ(target_header_integrity, *it->second->header_integrity());

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  // The content is loaded from PrefetchedSignedExchangeCache.
  NavigateToURLAndWaitTitle(target_sxg_url, "Prefetch Target (SXG)");
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       PrefetchedSignedExchangeCache_CrossOrigin) {
  int target_sxg_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";

  base::RunLoop target_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), target_sxg_path,
                         &target_sxg_fetch_count, &target_sxg_prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_sxg_url =
      embedded_test_server()->GetURL("sxg.example.com", target_sxg_path);
  const GURL target_url =
      embedded_test_server()->GetURL("publisher.example.com", target_path);

  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s'></body>",
                       target_sxg_url.spec().c_str())));
  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML content
      // as "application/signed-exchange;v=b3".
      ResponseEntry("<head><title>Prefetch Target (SXG)</title></head>",
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"}}));

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK, target_url,
      "text/html", {}, target_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  target_sxg_prefetch_waiter.Run();
  EXPECT_EQ(1, target_sxg_fetch_count);

  WaitUntilLoaded(target_sxg_url);

  EXPECT_EQ(1, prefetch_url_loader_called_);

  const auto cached_exchanges = GetCachedExchanges();
  EXPECT_EQ(1u, cached_exchanges.size());
  const auto it = cached_exchanges.find(target_sxg_url);
  ASSERT_TRUE(it != cached_exchanges.end());
  EXPECT_EQ(target_sxg_url, it->second->outer_url());
  EXPECT_EQ(target_url, it->second->inner_url());
  EXPECT_EQ(target_header_integrity, *it->second->header_integrity());

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  // The content is loaded from PrefetchedSignedExchangeCache.
  NavigateToURLAndWaitTitle(target_sxg_url, "Prefetch Target (SXG)");
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       PrefetchedSignedExchangeCache_SameUrl) {
  int target_sxg_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.html";
  const char* target_path = "/target.html";

  base::RunLoop target_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), target_sxg_path,
                         &target_sxg_fetch_count, &target_sxg_prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML content
      // as "application/signed-exchange;v=b3".
      ResponseEntry("<head><title>Prefetch Target (SXG)</title></head>",
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"}}));

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK, target_url,
      "text/html", {}, target_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  target_sxg_prefetch_waiter.Run();
  EXPECT_EQ(1, target_sxg_fetch_count);

  WaitUntilLoaded(target_sxg_url);

  EXPECT_EQ(1, prefetch_url_loader_called_);

  const auto cached_exchanges = GetCachedExchanges();
  EXPECT_EQ(1u, cached_exchanges.size());
  const auto it = cached_exchanges.find(target_sxg_url);
  ASSERT_TRUE(it != cached_exchanges.end());
  EXPECT_EQ(target_sxg_url, it->second->outer_url());
  EXPECT_EQ(target_url, it->second->inner_url());
  EXPECT_EQ(target_header_integrity, *it->second->header_integrity());

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  // The content is loaded from PrefetchedSignedExchangeCache.
  NavigateToURLAndWaitTitle(target_sxg_url, "Prefetch Target (SXG)");
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       PrefetchedSignedExchangeCache_BlobStorageLimit) {
  SetBlobLimits();
  int target_sxg_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";

  base::RunLoop target_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), target_sxg_path,
                         &target_sxg_fetch_count, &target_sxg_prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);
  std::string content = "<head><title>Prefetch Target (SXG)</title></head>";
  // Make the content larger than the disk space.
  content.resize(kTestBlobStorageMaxDiskSpace + 1, ' ');
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(target_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a HTML
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry(content, "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"}}));
  const net::SHA256HashValue target_header_integrity = {{0x01}};
  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK, target_url,
      "text/html", {}, target_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  target_sxg_prefetch_waiter.Run();
  EXPECT_EQ(1, target_sxg_fetch_count);

  WaitUntilLoaded(target_sxg_url);

  EXPECT_EQ(1, prefetch_url_loader_called_);

  const auto cached_exchanges = GetCachedExchanges();
  // The content of prefetched SXG is larger than the Blob storage limit.
  // So the SXG should not be stored to the cache.
  EXPECT_EQ(0u, cached_exchanges.size());
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       PrefetchAlternativeSubresourceSXG) {
  int script_sxg_fetch_count = 0;
  int script_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* script_sxg_path = "/script_js.sxg";
  const char* script_path = "/script.js";

  base::RunLoop script_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script_sxg_path,
                         &script_sxg_fetch_count, &script_sxg_prefetch_waiter);
  base::RunLoop script_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script_path,
                         &script_fetch_count, &script_prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);
  const GURL script_sxg_url = embedded_test_server()->GetURL(script_sxg_path);
  const GURL script_url = embedded_test_server()->GetURL(script_path);

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue script_header_integrity = {{0x02}};
  const std::string script_header_integrity_string =
      GetHeaderIntegrityString(script_header_integrity);

  const std::string outer_link_header = base::StringPrintf(
      "<%s>;"
      "rel=\"alternate\";"
      "type=\"application/signed-exchange;v=b3\";"
      "anchor=\"%s\"",
      script_sxg_url.spec().c_str(), script_url.spec().c_str());
  const std::string inner_link_headers = base::StringPrintf(
      "Link: "
      "<%s>;rel=\"allowed-alt-sxg\";header-integrity=\"%s\","
      "<%s>;rel=\"preload\";as=\"script\"",
      script_url.spec().c_str(), script_header_integrity_string.c_str(),
      script_url.spec().c_str());

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(script_path, ResponseEntry("document.title=\"from server\";",
                                              "text/javascript"));
  RegisterResponse(target_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a HTML
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("<head><title>Prefetch Target (SXG)</title>"
                                 "<script src=\"./script.js\"></script></head>",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"},
                                  {"link", outer_link_header}}));
  RegisterResponse(script_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a JS
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("document.title=\"done\";",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"}}));
  MockSignedExchangeHandlerFactory factory(
      {MockSignedExchangeHandlerParams(
           target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           target_url, "text/html", {inner_link_headers},
           target_header_integrity),
       MockSignedExchangeHandlerParams(
           script_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           script_url, "text/javascript", {}, script_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  script_sxg_prefetch_waiter.Run();
  EXPECT_EQ(1, script_sxg_fetch_count);
  EXPECT_EQ(0, script_fetch_count);

  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(script_sxg_url);

  const auto cached_exchanges = GetCachedExchanges();
  EXPECT_EQ(2u, cached_exchanges.size());

  const auto target_it = cached_exchanges.find(target_sxg_url);
  ASSERT_TRUE(target_it != cached_exchanges.end());
  EXPECT_EQ(target_sxg_url, target_it->second->outer_url());
  EXPECT_EQ(target_url, target_it->second->inner_url());
  EXPECT_EQ(target_header_integrity, *target_it->second->header_integrity());

  const auto script_it = cached_exchanges.find(script_sxg_url);
  ASSERT_TRUE(script_it != cached_exchanges.end());
  EXPECT_EQ(script_sxg_url, script_it->second->outer_url());
  EXPECT_EQ(script_url, script_it->second->inner_url());
  EXPECT_EQ(script_header_integrity, *script_it->second->header_integrity());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  // The content is loaded from PrefetchedSignedExchangeCache. And the script
  // is also loaded from PrefetchedSignedExchangeCache.
  NavigateToURLAndWaitTitle(target_sxg_url, "done");

  EXPECT_EQ(1, script_sxg_fetch_count);
  EXPECT_EQ(0, script_fetch_count);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       PrefetchAlternativeSubresourceSXG_ImageSrcsetAndSizes) {
  int image1_sxg_fetch_count = 0;
  int image1_fetch_count = 0;
  int image2_sxg_fetch_count = 0;
  int image2_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* image1_sxg_path = "/image1_png.sxg";
  const char* image1_path = "/image1.png";
  const char* image2_sxg_path = "/image2_png.sxg";
  const char* image2_path = "/image2.png";

  base::RunLoop image1_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), image1_sxg_path,
                         &image1_sxg_fetch_count, &image1_sxg_prefetch_waiter);
  base::RunLoop image1_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), image1_path,
                         &image1_fetch_count, &image1_prefetch_waiter);
  base::RunLoop image2_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), image2_sxg_path,
                         &image2_sxg_fetch_count, &image2_sxg_prefetch_waiter);
  base::RunLoop image2_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), image2_path,
                         &image2_fetch_count, &image2_prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);
  const GURL image1_sxg_url = embedded_test_server()->GetURL(image1_sxg_path);
  const GURL image1_url = embedded_test_server()->GetURL(image1_path);
  const GURL image2_sxg_url = embedded_test_server()->GetURL(image2_sxg_path);
  const GURL image2_url = embedded_test_server()->GetURL(image2_path);

  std::string image_contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath path;
    ASSERT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &path));
    path = path.AppendASCII("loader/empty16x16.png");
    ASSERT_TRUE(base::PathExists(path));
    ASSERT_TRUE(base::ReadFileToString(path, &image_contents));
  }

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue image1_header_integrity = {{0x02}};
  const net::SHA256HashValue image2_header_integrity = {{0x03}};
  const std::string image1_header_integrity_string =
      GetHeaderIntegrityString(image1_header_integrity);
  const std::string image2_header_integrity_string =
      GetHeaderIntegrityString(image2_header_integrity);

  const std::string outer_link_header = base::StringPrintf(
      "<%s>;"
      "rel=\"alternate\";"
      "type=\"application/signed-exchange;v=b3\";"
      "anchor=\"%s\","
      "<%s>;"
      "rel=\"alternate\";"
      "type=\"application/signed-exchange;v=b3\";"
      "anchor=\"%s\"",
      image1_sxg_url.spec().c_str(), image1_url.spec().c_str(),
      image2_sxg_url.spec().c_str(), image2_url.spec().c_str());
  const std::string inner_link_headers = base::StringPrintf(
      "Link: "
      "<%s>;rel=\"allowed-alt-sxg\";header-integrity=\"%s\","
      "<%s>;rel=\"allowed-alt-sxg\";header-integrity=\"%s\","
      "<%s>;rel=\"preload\";as=\"image\";"
      // imagesrcset says the size of image1 is 320, and the size of image2 is
      // 160.
      "imagesrcset=\"%s 320w, %s 160w\";"
      // imagesizes says the size of the image is 320. So image1 is selected.
      "imagesizes=\"320px\"",
      image1_url.spec().c_str(), image1_header_integrity_string.c_str(),
      image2_url.spec().c_str(), image2_header_integrity_string.c_str(),
      image2_url.spec().c_str(), image1_url.spec().c_str(),
      image2_url.spec().c_str());

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(image1_path, ResponseEntry(image_contents, "image/png"));
  RegisterResponse(image2_path, ResponseEntry(image_contents, "image/png"));
  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML
      // content as "application/signed-exchange;v=b3".
      ResponseEntry(base::StringPrintf(
                        "<head>"
                        "<title>Prefetch Target (SXG)</title>"
                        "</head>"
                        "<img src=\"%s\" onload=\"document.title='done'\">",
                        image1_url.spec().c_str()),
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"},
                     {"link", outer_link_header}}));
  RegisterResponse(
      image1_sxg_path,
      // We mock the SignedExchangeHandler, so just return a JS
      // content as "application/signed-exchange;v=b3".
      ResponseEntry(image_contents, "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"}}));
  MockSignedExchangeHandlerFactory factory(
      {MockSignedExchangeHandlerParams(
           target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           target_url, "text/html", {inner_link_headers},
           target_header_integrity),
       MockSignedExchangeHandlerParams(
           image1_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           image1_url, "image/png", {}, image1_header_integrity),
       MockSignedExchangeHandlerParams(
           image2_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           image2_url, "image/png", {}, image2_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  image1_sxg_prefetch_waiter.Run();
  EXPECT_EQ(1, image1_sxg_fetch_count);
  EXPECT_EQ(0, image1_fetch_count);
  EXPECT_EQ(0, image2_sxg_fetch_count);
  EXPECT_EQ(0, image2_fetch_count);

  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(image1_sxg_url);

  const auto cached_exchanges = GetCachedExchanges();
  EXPECT_EQ(2u, cached_exchanges.size());

  const auto target_it = cached_exchanges.find(target_sxg_url);
  ASSERT_TRUE(target_it != cached_exchanges.end());
  EXPECT_EQ(target_sxg_url, target_it->second->outer_url());
  EXPECT_EQ(target_url, target_it->second->inner_url());
  EXPECT_EQ(target_header_integrity, *target_it->second->header_integrity());

  const auto image1_it = cached_exchanges.find(image1_sxg_url);
  ASSERT_TRUE(image1_it != cached_exchanges.end());
  EXPECT_EQ(image1_sxg_url, image1_it->second->outer_url());
  EXPECT_EQ(image1_url, image1_it->second->inner_url());
  EXPECT_EQ(image1_header_integrity, *image1_it->second->header_integrity());

  NavigateToURLAndWaitTitle(target_sxg_url, "done");

  EXPECT_EQ(1, image1_sxg_fetch_count);
  EXPECT_EQ(0, image1_fetch_count);
  EXPECT_EQ(0, image2_sxg_fetch_count);
  EXPECT_EQ(0, image2_fetch_count);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       PrefetchAlternativeSubresourceSXG_CrossOrigin) {
  int script_sxg_fetch_count = 0;
  int script_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* script_sxg_path = "/script_js.sxg";
  const char* script_path = "/script.js";

  base::RunLoop script_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script_sxg_path,
                         &script_sxg_fetch_count, &script_sxg_prefetch_waiter);
  base::RunLoop script_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script_path,
                         &script_fetch_count, &script_prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_sxg_url =
      embedded_test_server()->GetURL("sxg.example.com", target_sxg_path);
  const GURL target_url =
      embedded_test_server()->GetURL("publisher.example.com", target_path);
  const GURL script_sxg_url =
      embedded_test_server()->GetURL("sxg.example.com", script_sxg_path);
  const GURL script_url =
      embedded_test_server()->GetURL("publisher.example.com", script_path);

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue script_header_integrity = {{0x02}};
  const std::string script_header_integrity_string =
      GetHeaderIntegrityString(script_header_integrity);

  const std::string outer_link_header = base::StringPrintf(
      "<%s>;"
      "rel=\"alternate\";"
      "type=\"application/signed-exchange;v=b3\";"
      "anchor=\"%s\"",
      script_sxg_url.spec().c_str(), script_url.spec().c_str());
  const std::string inner_link_headers = base::StringPrintf(
      "Link: "
      "<%s>;rel=\"allowed-alt-sxg\";header-integrity=\"%s\","
      "<%s>;rel=\"preload\";as=\"script\"",
      script_url.spec().c_str(), script_header_integrity_string.c_str(),
      script_url.spec().c_str());

  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s'></body>",
                       target_sxg_url.spec().c_str())));
  RegisterResponse(script_path, ResponseEntry("document.title=\"from server\";",
                                              "text/javascript"));
  RegisterResponse(target_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a HTML
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("<head><title>Prefetch Target (SXG)</title>"
                                 "<script src=\"./script.js\"></script></head>",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"},
                                  {"link", outer_link_header}}));
  RegisterResponse(script_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a JS
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("document.title=\"done\";",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"}}));
  MockSignedExchangeHandlerFactory factory(
      {MockSignedExchangeHandlerParams(
           target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           target_url, "text/html", {inner_link_headers},
           target_header_integrity),
       MockSignedExchangeHandlerParams(
           script_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           script_url, "text/javascript", {}, script_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  script_sxg_prefetch_waiter.Run();
  EXPECT_EQ(1, script_sxg_fetch_count);
  EXPECT_EQ(0, script_fetch_count);

  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(script_sxg_url);

  const auto cached_exchanges = GetCachedExchanges();
  EXPECT_EQ(2u, cached_exchanges.size());

  const auto target_it = cached_exchanges.find(target_sxg_url);
  ASSERT_TRUE(target_it != cached_exchanges.end());
  EXPECT_EQ(target_sxg_url, target_it->second->outer_url());
  EXPECT_EQ(target_url, target_it->second->inner_url());
  EXPECT_EQ(target_header_integrity, *target_it->second->header_integrity());

  const auto script_it = cached_exchanges.find(script_sxg_url);
  ASSERT_TRUE(script_it != cached_exchanges.end());
  EXPECT_EQ(script_sxg_url, script_it->second->outer_url());
  EXPECT_EQ(script_url, script_it->second->inner_url());
  EXPECT_EQ(script_header_integrity, *script_it->second->header_integrity());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  // The content is loaded from PrefetchedSignedExchangeCache. And the script
  // is also loaded from PrefetchedSignedExchangeCache.
  NavigateToURLAndWaitTitle(target_sxg_url, "done");

  EXPECT_EQ(1, script_sxg_fetch_count);
  EXPECT_EQ(0, script_fetch_count);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       PrefetchAlternativeSubresourceSXG_DifferentOrigin) {
  int script_sxg_fetch_count = 0;
  int script_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* script_sxg_path = "/script_js.sxg";
  const char* script_path = "/script.js";

  base::RunLoop script_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script_sxg_path,
                         &script_sxg_fetch_count, &script_sxg_prefetch_waiter);
  base::RunLoop script_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script_path,
                         &script_fetch_count, &script_prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);
  // script_js.sxg is served from different origin.
  const GURL script_sxg_url =
      embedded_test_server()->GetURL("example.com", script_sxg_path);
  const GURL script_url = embedded_test_server()->GetURL(script_path);

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue script_header_integrity = {{0x02}};
  const std::string script_header_integrity_string =
      GetHeaderIntegrityString(script_header_integrity);

  const std::string outer_link_header = base::StringPrintf(
      "<%s>;"
      "rel=\"alternate\";"
      "type=\"application/signed-exchange;v=b3\";"
      "anchor=\"%s\"",
      script_sxg_url.spec().c_str(), script_url.spec().c_str());
  const std::string inner_link_headers = base::StringPrintf(
      "Link: "
      "<%s>;rel=\"allowed-alt-sxg\";header-integrity=\"%s\","
      "<%s>;rel=\"preload\";as=\"script\"",
      script_url.spec().c_str(), script_header_integrity_string.c_str(),
      script_url.spec().c_str());

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(script_path, ResponseEntry("document.title=\"from server\";",
                                              "text/javascript"));
  RegisterResponse(target_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a HTML
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("<head><title>Prefetch Target (SXG)</title>"
                                 "<script src=\"./script.js\"></script></head>",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"},
                                  {"link", outer_link_header}}));
  RegisterResponse(script_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a JS
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("document.title=\"done\";",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"}}));
  MockSignedExchangeHandlerFactory factory(
      {MockSignedExchangeHandlerParams(
           target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           target_url, "text/html", {inner_link_headers},
           target_header_integrity),
       MockSignedExchangeHandlerParams(
           script_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           script_url, "text/javascript", {}, script_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  script_sxg_prefetch_waiter.Run();
  EXPECT_EQ(1, script_sxg_fetch_count);
  EXPECT_EQ(0, script_fetch_count);

  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(script_sxg_url);

  const auto cached_exchanges = GetCachedExchanges();
  EXPECT_EQ(2u, cached_exchanges.size());

  const auto target_it = cached_exchanges.find(target_sxg_url);
  ASSERT_TRUE(target_it != cached_exchanges.end());
  EXPECT_EQ(target_sxg_url, target_it->second->outer_url());
  EXPECT_EQ(target_url, target_it->second->inner_url());
  EXPECT_EQ(target_header_integrity, *target_it->second->header_integrity());

  const auto script_it = cached_exchanges.find(script_sxg_url);
  ASSERT_TRUE(script_it != cached_exchanges.end());
  EXPECT_EQ(script_sxg_url, script_it->second->outer_url());
  EXPECT_EQ(script_url, script_it->second->inner_url());
  EXPECT_EQ(script_header_integrity, *script_it->second->header_integrity());

  // script_js.sxg was served from different origin from target.sxg. So
  // script.js is loaded from the server.
  NavigateToURLAndWaitTitle(target_sxg_url, "from server");

  EXPECT_EQ(1, script_sxg_fetch_count);
  EXPECT_EQ(1, script_fetch_count);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       PrefetchAlternativeSubresourceSXG_MultipleResources) {
  int script1_sxg_fetch_count = 0;
  int script1_fetch_count = 0;
  int script2_sxg_fetch_count = 0;
  int script2_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* script1_sxg_path = "/script1_js.sxg";
  const char* script1_path = "/script1.js";
  const char* script2_sxg_path = "/script2_js.sxg";
  const char* script2_path = "/script2.js";

  base::RunLoop script1_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script1_sxg_path,
                         &script1_sxg_fetch_count,
                         &script1_sxg_prefetch_waiter);
  base::RunLoop script1_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script1_path,
                         &script1_fetch_count, &script1_prefetch_waiter);
  base::RunLoop script2_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script2_sxg_path,
                         &script2_sxg_fetch_count,
                         &script2_sxg_prefetch_waiter);
  base::RunLoop script2_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script2_path,
                         &script2_fetch_count, &script2_prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);
  const GURL script1_sxg_url = embedded_test_server()->GetURL(script1_sxg_path);
  const GURL script1_url = embedded_test_server()->GetURL(script1_path);
  const GURL script2_sxg_url = embedded_test_server()->GetURL(script2_sxg_path);
  const GURL script2_url = embedded_test_server()->GetURL(script2_path);

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue script1_header_integrity = {{0x02}};
  const std::string script1_header_integrity_string =
      GetHeaderIntegrityString(script1_header_integrity);
  const net::SHA256HashValue script2_header_integrity = {{0x03}};
  const std::string script2_header_integrity_string =
      GetHeaderIntegrityString(script2_header_integrity);

  const std::string outer_link_header = base::StringPrintf(
      "<%s>;rel=\"alternate\";type=\"application/signed-exchange;v=b3\";"
      "anchor=\"%s\","
      "<%s>;rel=\"alternate\";type=\"application/signed-exchange;v=b3\";"
      "anchor=\"%s\"",
      script1_sxg_url.spec().c_str(), script1_url.spec().c_str(),
      script2_sxg_url.spec().c_str(), script2_url.spec().c_str());
  const std::string inner_link_headers = base::StringPrintf(
      "Link: "
      "<%s>;rel=\"allowed-alt-sxg\";header-integrity=\"%s\","
      "<%s>;rel=\"preload\";as=\"script\","
      "<%s>;rel=\"allowed-alt-sxg\";header-integrity=\"%s\","
      "<%s>;rel=\"preload\";as=\"script\"",
      script1_url.spec().c_str(), script1_header_integrity_string.c_str(),
      script1_url.spec().c_str(), script2_url.spec().c_str(),
      script2_header_integrity_string.c_str(), script2_url.spec().c_str());

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(script1_path, ResponseEntry("var test_title=\"from\";",
                                               "text/javascript"));
  RegisterResponse(script2_path,
                   ResponseEntry("document.title=test_title+\"server\";",
                                 "text/javascript"));
  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML
      // content as "application/signed-exchange;v=b3".
      ResponseEntry("<head><title>Prefetch Target (SXG)</title>"
                    "<script src=\"./script1.js\"></script>"
                    "<script src=\"./script2.js\"></script></head>",
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"},
                     {"link", outer_link_header}}));
  RegisterResponse(script1_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a JS
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("var test_title=\"done\";",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"}}));
  RegisterResponse(script2_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a JS
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("document.title=test_title;",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"}}));
  MockSignedExchangeHandlerFactory factory({
      MockSignedExchangeHandlerParams(
          target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
          target_url, "text/html", {inner_link_headers},
          target_header_integrity),
      MockSignedExchangeHandlerParams(
          script1_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
          script1_url, "text/javascript", {}, script1_header_integrity),
      MockSignedExchangeHandlerParams(
          script2_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
          script2_url, "text/javascript", {}, script2_header_integrity),
  });
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  script1_sxg_prefetch_waiter.Run();
  script2_sxg_prefetch_waiter.Run();
  EXPECT_EQ(1, script1_sxg_fetch_count);
  EXPECT_EQ(0, script1_fetch_count);
  EXPECT_EQ(1, script2_sxg_fetch_count);
  EXPECT_EQ(0, script2_fetch_count);

  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(script1_sxg_url);
  WaitUntilLoaded(script2_sxg_url);

  const auto cached_exchanges = GetCachedExchanges();
  EXPECT_EQ(3u, cached_exchanges.size());

  const auto target_it = cached_exchanges.find(target_sxg_url);
  ASSERT_TRUE(target_it != cached_exchanges.end());
  EXPECT_EQ(target_sxg_url, target_it->second->outer_url());
  EXPECT_EQ(target_url, target_it->second->inner_url());
  EXPECT_EQ(target_header_integrity, *target_it->second->header_integrity());

  const auto script1_it = cached_exchanges.find(script1_sxg_url);
  ASSERT_TRUE(script1_it != cached_exchanges.end());
  EXPECT_EQ(script1_sxg_url, script1_it->second->outer_url());
  EXPECT_EQ(script1_url, script1_it->second->inner_url());
  EXPECT_EQ(script1_header_integrity, *script1_it->second->header_integrity());

  const auto script2_it = cached_exchanges.find(script2_sxg_url);
  ASSERT_TRUE(script2_it != cached_exchanges.end());
  EXPECT_EQ(script2_sxg_url, script2_it->second->outer_url());
  EXPECT_EQ(script2_url, script2_it->second->inner_url());
  EXPECT_EQ(script2_header_integrity, *script2_it->second->header_integrity());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  // The content is loaded from PrefetchedSignedExchangeCache. And the scripts
  // are also loaded from PrefetchedSignedExchangeCache.
  NavigateToURLAndWaitTitle(target_sxg_url, "done");

  EXPECT_EQ(1, script1_sxg_fetch_count);
  EXPECT_EQ(0, script1_fetch_count);
  EXPECT_EQ(1, script2_sxg_fetch_count);
  EXPECT_EQ(0, script2_fetch_count);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       PrefetchAlternativeSubresourceSXG_SameUrl) {
  int script_sxg_fetch_count = 0;
  int script_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.html";
  const char* target_path = "/target.html";
  const char* script_sxg_path = "/script.js";
  const char* script_path = "/script.js";

  base::RunLoop script_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script_sxg_path,
                         &script_sxg_fetch_count, &script_sxg_prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);
  const GURL script_sxg_url = embedded_test_server()->GetURL(script_sxg_path);
  const GURL script_url = embedded_test_server()->GetURL(script_path);

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue script_header_integrity = {{0x02}};
  const std::string script_header_integrity_string =
      GetHeaderIntegrityString(script_header_integrity);

  const std::string outer_link_header = base::StringPrintf(
      "<%s>;"
      "rel=\"alternate\";"
      "type=\"application/signed-exchange;v=b3\";"
      "anchor=\"%s\"",
      script_sxg_url.spec().c_str(), script_url.spec().c_str());
  const std::string inner_link_headers = base::StringPrintf(
      "Link: "
      "<%s>;rel=\"allowed-alt-sxg\";header-integrity=\"%s\","
      "<%s>;rel=\"preload\";as=\"script\"",
      script_url.spec().c_str(), script_header_integrity_string.c_str(),
      script_url.spec().c_str());

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(script_path, ResponseEntry("document.title=\"from server\";",
                                              "text/javascript"));
  RegisterResponse(target_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a HTML
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("<head><title>Prefetch Target (SXG)</title>"
                                 "<script src=\"./script.js\"></script></head>",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"},
                                  {"link", outer_link_header}}));
  RegisterResponse(script_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a JS
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("document.title=\"done\";",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"}}));
  MockSignedExchangeHandlerFactory factory(
      {MockSignedExchangeHandlerParams(
           target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           target_url, "text/html", {inner_link_headers},
           target_header_integrity),
       MockSignedExchangeHandlerParams(
           script_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           script_url, "text/javascript", {}, script_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  script_sxg_prefetch_waiter.Run();
  EXPECT_EQ(1, script_sxg_fetch_count);
  EXPECT_EQ(0, script_fetch_count);

  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(script_sxg_url);

  const auto cached_exchanges = GetCachedExchanges();
  EXPECT_EQ(2u, cached_exchanges.size());

  const auto target_it = cached_exchanges.find(target_sxg_url);
  ASSERT_TRUE(target_it != cached_exchanges.end());
  EXPECT_EQ(target_sxg_url, target_it->second->outer_url());
  EXPECT_EQ(target_url, target_it->second->inner_url());
  EXPECT_EQ(target_header_integrity, *target_it->second->header_integrity());

  const auto script_it = cached_exchanges.find(script_sxg_url);
  ASSERT_TRUE(script_it != cached_exchanges.end());
  EXPECT_EQ(script_sxg_url, script_it->second->outer_url());
  EXPECT_EQ(script_url, script_it->second->inner_url());
  EXPECT_EQ(script_header_integrity, *script_it->second->header_integrity());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  // The content is loaded from PrefetchedSignedExchangeCache. And the script
  // is also loaded from PrefetchedSignedExchangeCache.
  NavigateToURLAndWaitTitle(target_sxg_url, "done");
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       PrefetchAlternativeSubresourceSXG_IntegrityMismatch) {
  int script_sxg_fetch_count = 0;
  int script_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* script_path = "/script.js";
  const char* script_sxg_path = "/script_js.sxg";

  base::RunLoop script_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script_sxg_path,
                         &script_sxg_fetch_count, &script_sxg_prefetch_waiter);
  base::RunLoop script_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script_path,
                         &script_fetch_count, &script_prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);
  const GURL script_sxg_url = embedded_test_server()->GetURL(script_sxg_path);
  const GURL script_url = embedded_test_server()->GetURL(script_path);

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue script_header_integrity = {{0x02}};
  const net::SHA256HashValue wrong_script_header_integrity = {{0x03}};
  // Use the wrong header integrity value for "allowed-alt-sxg" link header to
  // trigger the integrity mismatch fallback logic.
  const std::string script_header_integrity_string =
      GetHeaderIntegrityString(wrong_script_header_integrity);

  const std::string outer_link_header = base::StringPrintf(
      "<%s>;"
      "rel=\"alternate\";"
      "type=\"application/signed-exchange;v=b3\";"
      "anchor=\"%s\"",
      script_sxg_url.spec().c_str(), script_url.spec().c_str());
  const std::string inner_link_headers = base::StringPrintf(
      "Link: "
      "<%s>;rel=\"allowed-alt-sxg\";header-integrity=\"%s\","
      "<%s>;rel=\"preload\";as=\"script\"",
      script_url.spec().c_str(), script_header_integrity_string.c_str(),
      script_url.spec().c_str());

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(script_path, ResponseEntry("document.title=\"from server\";",
                                              "text/javascript"));
  RegisterResponse(target_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a HTML
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("<head><title>Prefetch Target (SXG)</title>"
                                 "<script src=\"./script.js\"></script></head>",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"},
                                  {"link", outer_link_header}}));
  RegisterResponse(script_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a JS
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("document.title=\"done\";",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"}}));
  MockSignedExchangeHandlerFactory factory(
      {MockSignedExchangeHandlerParams(
           target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           target_url, "text/html", {inner_link_headers},
           target_header_integrity),
       MockSignedExchangeHandlerParams(
           script_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           script_url, "text/javascript", {}, script_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  script_sxg_prefetch_waiter.Run();
  EXPECT_EQ(1, script_sxg_fetch_count);
  EXPECT_EQ(0, script_fetch_count);

  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(script_sxg_url);

  // The value of "header-integrity" in "allowed-alt-sxg" link header of the
  // inner response doesn't match the actual header integrity of script_js.sxg.
  // So the script request must go to the server.
  NavigateToURLAndWaitTitle(target_sxg_url, "from server");

  EXPECT_EQ(1, script_fetch_count);
}

IN_PROC_BROWSER_TEST_P(
    SignedExchangeSubresourcePrefetchBrowserTest,
    PrefetchAlternativeSubresourceSXG_MultipleResources_IntegrityMismatch) {
  int script1_sxg_fetch_count = 0;
  int script1_fetch_count = 0;
  int script2_sxg_fetch_count = 0;
  int script2_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* script1_sxg_path = "/script1_js.sxg";
  const char* script1_path = "/script1.js";
  const char* script2_sxg_path = "/script2_js.sxg";
  const char* script2_path = "/script2.js";

  base::RunLoop script1_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script1_sxg_path,
                         &script1_sxg_fetch_count,
                         &script1_sxg_prefetch_waiter);
  base::RunLoop script1_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script1_path,
                         &script1_fetch_count, &script1_prefetch_waiter);
  base::RunLoop script2_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script2_sxg_path,
                         &script2_sxg_fetch_count,
                         &script2_sxg_prefetch_waiter);
  base::RunLoop script2_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script2_path,
                         &script2_fetch_count, &script2_prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);
  const GURL script1_sxg_url = embedded_test_server()->GetURL(script1_sxg_path);
  const GURL script1_url = embedded_test_server()->GetURL(script1_path);
  const GURL script2_sxg_url = embedded_test_server()->GetURL(script2_sxg_path);
  const GURL script2_url = embedded_test_server()->GetURL(script2_path);

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue script1_header_integrity = {{0x02}};
  const std::string script1_header_integrity_string =
      GetHeaderIntegrityString(script1_header_integrity);
  const net::SHA256HashValue script2_header_integrity = {{0x03}};
  const net::SHA256HashValue wrong_script2_header_integrity = {{0x04}};
  // Use the wrong header integrity value for "allowed-alt-sxg" link header to
  // trigger the integrity mismatch fallback logic.
  const std::string script2_header_integrity_string =
      GetHeaderIntegrityString(wrong_script2_header_integrity);

  const std::string outer_link_header = base::StringPrintf(
      "<%s>;rel=\"alternate\";type=\"application/signed-exchange;v=b3\";"
      "anchor=\"%s\","
      "<%s>;rel=\"alternate\";type=\"application/signed-exchange;v=b3\";"
      "anchor=\"%s\"",
      script1_sxg_url.spec().c_str(), script1_url.spec().c_str(),
      script2_sxg_url.spec().c_str(), script2_url.spec().c_str());
  const std::string inner_link_headers = base::StringPrintf(
      "Link: "
      "<%s>;rel=\"allowed-alt-sxg\";header-integrity=\"%s\","
      "<%s>;rel=\"preload\";as=\"script\","
      "<%s>;rel=\"allowed-alt-sxg\";header-integrity=\"%s\","
      "<%s>;rel=\"preload\";as=\"script\"",
      script1_url.spec().c_str(), script1_header_integrity_string.c_str(),
      script1_url.spec().c_str(), script2_url.spec().c_str(),
      script2_header_integrity_string.c_str(), script2_url.spec().c_str());

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(script1_path, ResponseEntry("var test_title=\"from\";",
                                               "text/javascript"));
  RegisterResponse(script2_path,
                   ResponseEntry("document.title=test_title+\" server\";",
                                 "text/javascript"));
  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML
      // content as "application/signed-exchange;v=b3".
      ResponseEntry("<head><title>Prefetch Target (SXG)</title>"
                    "<script src=\"./script1.js\"></script>"
                    "<script src=\"./script2.js\"></script></head>",
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"},
                     {"link", outer_link_header}}));
  RegisterResponse(script1_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a JS
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("var test_title=\"done\";",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"}}));
  RegisterResponse(script2_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a JS
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("document.title=test_title;",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"}}));
  MockSignedExchangeHandlerFactory factory({
      MockSignedExchangeHandlerParams(
          target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
          target_url, "text/html", {inner_link_headers},
          target_header_integrity),
      MockSignedExchangeHandlerParams(
          script1_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
          script1_url, "text/javascript", {}, script1_header_integrity),
      MockSignedExchangeHandlerParams(
          script2_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
          script2_url, "text/javascript", {}, script2_header_integrity),
  });
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));
  script1_sxg_prefetch_waiter.Run();
  script2_sxg_prefetch_waiter.Run();
  EXPECT_EQ(1, script1_sxg_fetch_count);
  EXPECT_EQ(0, script1_fetch_count);
  EXPECT_EQ(1, script2_sxg_fetch_count);
  EXPECT_EQ(0, script2_fetch_count);

  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(script1_sxg_url);
  WaitUntilLoaded(script2_sxg_url);

  const auto cached_exchanges = GetCachedExchanges();
  EXPECT_EQ(3u, cached_exchanges.size());

  // The value of "header-integrity" in "allowed-alt-sxg" link header of the
  // inner response doesn't match the actual header integrity of script2_js.sxg.
  // So the all script requests must go to the server.
  NavigateToURLAndWaitTitle(target_sxg_url, "from server");

  EXPECT_EQ(1, script1_sxg_fetch_count);
  EXPECT_EQ(1, script1_fetch_count);
  EXPECT_EQ(1, script2_sxg_fetch_count);
  EXPECT_EQ(1, script2_fetch_count);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest, CORS) {
  std::unique_ptr<net::EmbeddedTestServer> data_server =
      std::make_unique<net::EmbeddedTestServer>(
          net::EmbeddedTestServer::TYPE_HTTPS);

  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";

  RegisterRequestHandler(embedded_test_server());
  RegisterRequestHandler(data_server.get());
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(data_server->Start());

  std::string test_server_origin = embedded_test_server()->base_url().spec();
  // Remove the last "/""
  test_server_origin.pop_back();

  struct {
    // Set in the main SXG's inner response header.
    // Example:
    //   Link: <http://***/**.data>;rel="preload";as="fetch";crossorigin
    //                                                       ^^^^^^^^^^^
    const char* const crossorigin_preload_header;
    // Set in the data SXG's inner response header.
    // Example:
    //   Access-Control-Allow-Origin: *
    //                                ^
    const char* const access_control_allow_origin_header;
    // Set "Access-Control-Allow-Credentials: true" link header in the data
    // SXG's inner response header.
    const bool has_access_control_allow_credentials_true_header;
    // The credentials attribute of Fetch API's request while fetching the data.
    const char* const request_credentials;
    // If the data is served from the SXG the result must be "sxg". If the data
    // is served from the server without SXG, the result must be "server". If
    // failed to fetch the data, the result must be "failed".
    const char* const expected_result;
  } kTestCases[] = {
      // - If crossorigin is not set in the preload header, cross origin fetch
      //   goes to the server. It is because the mode of the preload request
      //   ("no-cors") and the mode of the fetch request ("cors") doesn't match.
      {nullptr, nullptr, false, "omit", "server"},
      {nullptr, nullptr, false, "include", "server"},
      {nullptr, nullptr, false, "same-origin", "server"},

      // - When "crossorigin" is set in the preload header, the mode of the
      //   preload request is "cors", and the credentials mode is "same-origin".
      //   - When the credentials mode of the fetch request doesn't match, the
      //     fetch request goes to the server.
      {"crossorigin", nullptr, false, "omit", "server"},
      {"crossorigin", nullptr, false, "include", "server"},
      //   - When the credentials mode of the fetch request match with the
      //     credentials mode of the preload request, the prefetched signed
      //     exchange is used.
      //     - When the inner response doesn't have ACAO header, fails to load.
      {"crossorigin", nullptr, false, "same-origin", "failed"},
      //     - When the inner response has "ACAO: *" header, succeeds to load.
      {"crossorigin", "*", false, "same-origin", "sxg"},
      //     - When the inner response has "ACAO: <origin>" header, succeeds to
      //       load.
      {"crossorigin", test_server_origin.c_str(), false, "same-origin", "sxg"},

      // - crossorigin="anonymous" must be treated as same as just having
      //   "crossorigin".
      {"crossorigin=\"anonymous\"", nullptr, false, "omit", "server"},
      {"crossorigin=\"anonymous\"", nullptr, false, "include", "server"},
      {"crossorigin=\"anonymous\"", nullptr, false, "same-origin", "failed"},
      {"crossorigin=\"anonymous\"", "*", false, "same-origin", "sxg"},
      {"crossorigin=\"anonymous\"", test_server_origin.c_str(), false,
       "same-origin", "sxg"},

      // - When crossorigin="use-credentials" is set in the preload header, the
      //   mode of the preload request is "cors", and the credentials mode is
      //   "include".
      //  - When the credentials mode of the fetch request doesn't match, the
      //    fetch request goes to the server.
      {"crossorigin=\"use-credentials\"", nullptr, false, "omit", "server"},
      {"crossorigin=\"use-credentials\"", nullptr, false, "same-origin",
       "server"},
      //   - When the credentials mode of the fetch request match with the
      //     credentials mode of the preload request, the prefetched signed
      //     exchange is used.
      //     - When the inner response doesn't have ACAO header, fails to load.
      {"crossorigin=\"use-credentials\"", nullptr, false, "include", "failed"},
      //     - Even if the inner response has "ACAO: *" header, fails to load
      //       the "include" credentials mode request.
      {"crossorigin=\"use-credentials\"", "*", false, "include", "failed"},
      //     - Even if the inner response has "ACAO: *" header and "ACAC: true"
      //       header, fails to load the "include" credentials mode request.
      {"crossorigin=\"use-credentials\"", "*", true, "include", "failed"},
      //     - Even if the inner response has "ACAO: <origin>" header, fails to
      //       load the "include" credentials mode request.
      {"crossorigin=\"use-credentials\"", test_server_origin.c_str(), false,
       "include", "failed"},
      //     - If the inner response has "ACAO: <origin>" header, and
      //       "ACAC: true" header, succeeds to load.
      {"crossorigin=\"use-credentials\"", test_server_origin.c_str(), true,
       "include", "sxg"},
  };

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);

  const net::SHA256HashValue target_header_integrity = {{0x01}};

  std::string target_sxg_outer_link_header;
  std::string target_sxg_inner_link_header("Link: ");
  std::string requests_list_string;

  std::vector<MockSignedExchangeHandlerParams> mock_params;
  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    if (i) {
      target_sxg_outer_link_header += ",";
      target_sxg_inner_link_header += ",";
      requests_list_string += ",";
    }
    const std::string data_sxg_path = base::StringPrintf("/%zu_data.sxg", i);
    const std::string data_path = base::StringPrintf("/%zu.data", i);
    const GURL data_sxg_url = embedded_test_server()->GetURL(data_sxg_path);
    const GURL data_url = data_server->GetURL(data_path);
    requests_list_string += base::StringPrintf(
        "new Request('%s', {credentials: '%s'})", data_url.spec().c_str(),
        kTestCases[i].request_credentials);
    const net::SHA256HashValue data_header_integrity = {{0x02 + i}};
    const std::string data_header_integrity_string =
        GetHeaderIntegrityString(data_header_integrity);

    target_sxg_outer_link_header += base::StringPrintf(
        "<%s>;rel=\"alternate\";type=\"application/signed-exchange;v=b3\";"
        "anchor=\"%s\"",
        data_sxg_url.spec().c_str(), data_url.spec().c_str());

    target_sxg_inner_link_header += base::StringPrintf(
        "<%s>;rel=\"allowed-alt-sxg\";header-integrity=\"%s\","
        "<%s>;rel=\"preload\";as=\"fetch\"",
        data_url.spec().c_str(), data_header_integrity_string.c_str(),
        data_url.spec().c_str());
    if (kTestCases[i].crossorigin_preload_header) {
      target_sxg_inner_link_header +=
          base::StringPrintf(";%s", kTestCases[i].crossorigin_preload_header);
    }
    RegisterResponse(data_sxg_path,
                     ResponseEntry("sxg", "application/signed-exchange;v=b3",
                                   {{"x-content-type-options", "nosniff"}}));
    RegisterResponse(
        data_path,
        ResponseEntry(
            "server", "text/plain",
            {{"Access-Control-Allow-Origin", test_server_origin.c_str()},
             {"Access-Control-Allow-Credentials", "true"}}));
    std::vector<std::string> data_sxg_inner_headers;
    if (kTestCases[i].access_control_allow_origin_header) {
      data_sxg_inner_headers.emplace_back(
          base::StringPrintf("Access-Control-Allow-Origin: %s",
                             kTestCases[i].access_control_allow_origin_header));
    }
    if (kTestCases[i].has_access_control_allow_credentials_true_header) {
      data_sxg_inner_headers.emplace_back(
          "Access-Control-Allow-Credentials: true");
    }
    mock_params.emplace_back(
        data_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK, data_url,
        "text/plain", std::move(data_sxg_inner_headers), data_header_integrity);
  }

  std::vector<std::string> target_sxg_inner_headers = {
      std::move(target_sxg_inner_link_header)};
  mock_params.emplace_back(target_sxg_url, SignedExchangeLoadResult::kSuccess,
                           net::OK, target_url, "text/html",
                           std::move(target_sxg_inner_headers),
                           target_header_integrity);
  MockSignedExchangeHandlerFactory factory(std::move(mock_params));

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML
      // content as "application/signed-exchange;v=b3".
      ResponseEntry(base::StringPrintf(R"(
<head><title>Prefetch Target (SXG)</title><script>
let results = [];
(async function(requests) {
  for (let i = 0; i < requests.length; ++i) {
    try {
      const res = await fetch(requests[i]);
      results.push(await res.text());
    } catch (err) {
      results.push('failed');
    }
  }
  document.title = 'done';
})([%s]);
</script></head>)",
                                       requests_list_string.c_str()),
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"},
                     {"link", target_sxg_outer_link_header}}));

  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path));

  // Wait until all (main- and sub-resource) SXGs are prefetched.
  while (GetCachedExchanges().size() < base::size(kTestCases) + 1) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  NavigateToURLAndWaitTitle(target_sxg_url, "done");

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    SCOPED_TRACE(base::StringPrintf("TestCase %zu", i));
    EXPECT_EQ(
        EvalJs(shell(), base::StringPrintf("results[%zu]", i)).ExtractString(),
        kTestCases[i].expected_result);
  }
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       CrossOriginReadBlocking_AllowedAfterSniffing) {
  RunCrossOriginReadBlockingTest(
      true /* cross_origin */, "document.title=\"done\"", "text/javascript",
      false /* has_nosniff */, "done",
      network::CrossOriginReadBlocking::Action::kAllowedAfterSniffing);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       CrossOriginReadBlocking_BlockedAfterSniffing) {
  RunCrossOriginReadBlockingTest(
      true /* cross_origin */, "<!DOCTYPE html>hello;", "text/html",
      false /* has_nosniff */, "original title",
      network::CrossOriginReadBlocking::Action::kBlockedAfterSniffing);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       CrossOriginReadBlocking_AllowedWithoutSniffing) {
  RunCrossOriginReadBlockingTest(
      false /* cross_origin */, "document.title=\"done\"", "text/javascript",
      true /* has_nosniff */, "done",
      network::CrossOriginReadBlocking::Action::kAllowedWithoutSniffing);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       CrossOriginReadBlocking_BlockedWithoutSniffing) {
  RunCrossOriginReadBlockingTest(
      true /* cross_origin */, "<!DOCTYPE html>hello;", "text/html",
      true /* has_nosniff */, "original title",
      network::CrossOriginReadBlocking::Action::kBlockedWithoutSniffing);
}

INSTANTIATE_TEST_SUITE_P(
    SignedExchangeSubresourcePrefetchBrowserTest,
    SignedExchangeSubresourcePrefetchBrowserTest,
    testing::Values(SignedExchangeSubresourcePrefetchBrowserTestParam(false),
                    SignedExchangeSubresourcePrefetchBrowserTestParam(true)));
}  // namespace content
