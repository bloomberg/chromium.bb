// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
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
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "services/network/public/cpp/features.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

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

 protected:
  static constexpr size_t kTestBlobStorageIPCThresholdBytes = 20;
  static constexpr size_t kTestBlobStorageMaxSharedMemoryBytes = 50;
  static constexpr size_t kTestBlobStorageMaxBlobMemorySize = 400;
  static constexpr uint64_t kTestBlobStorageMaxDiskSpace = 500;
  static constexpr uint64_t kTestBlobStorageMinFileSizeBytes = 10;
  static constexpr uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

  const PrefetchedSignedExchangeCache::EntryMap& GetCachedExchanges() {
    RenderViewHost* rvh = shell()->web_contents()->GetRenderViewHost();
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(rvh->GetMainFrame());
    scoped_refptr<PrefetchedSignedExchangeCache> cache =
        rfh->EnsurePrefetchedSignedExchangeCache();
    return cache->exchanges_;
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

  const auto& cached_exchanges = GetCachedExchanges();
  EXPECT_EQ(1u, cached_exchanges.size());
  const auto it = cached_exchanges.find(target_sxg_url);
  ASSERT_TRUE(it != cached_exchanges.end());
  EXPECT_EQ(target_sxg_url, it->second->outer_url());
  EXPECT_EQ(target_url, it->second->inner_url());
  EXPECT_EQ(target_header_integrity, *it->second->header_integrity());
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

  const auto& cached_exchanges = GetCachedExchanges();
  // The content of prefetched SXG is larger than the Blob storage limit.
  // So the SXG should not be stored to the cache.
  EXPECT_EQ(0u, cached_exchanges.size());
}

IN_PROC_BROWSER_TEST_P(SignedExchangeSubresourcePrefetchBrowserTest,
                       PrefetchAlternativeSubresourceSXG) {
  int script_sxg_fetch_count = 0;
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* script_path_in_sxg = "/script.js";
  const char* script_sxg_path = "/script_js.sxg";

  base::RunLoop script_sxg_prefetch_waiter;
  RegisterRequestMonitor(embedded_test_server(), script_sxg_path,
                         &script_sxg_fetch_count, &script_sxg_prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, prefetch_url_loader_called_);

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);
  const GURL script_sxg_url = embedded_test_server()->GetURL(script_sxg_path);
  const GURL script_url = embedded_test_server()->GetURL(script_path_in_sxg);

  const std::string outer_link_header = base::StringPrintf(
      "<%s>;"
      "rel=\"alternate\";"
      "type=\"application/signed-exchange;v=b3\";"
      "anchor=\"%s\"",
      script_sxg_url.spec().c_str(), script_url.spec().c_str());
  const std::string inner_link_headers = base::StringPrintf(
      "Link: <%s>;"
      "rel=\"allowed-alt-sxg\";header-integrity=\"%s\","
      "<%s>;rel=\"preload\";as=\"script\"",
      script_url.spec().c_str(),
      // This is just a dummy data as of now.
      // TODO(crbug.com/935267): When we will implement the header integrity
      // checking logic, add tests for it.
      "sha256-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
      script_url.spec().c_str());

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
                    {{"x-content-type-options", "nosniff"},
                     {"link", outer_link_header}}));
  RegisterResponse(script_sxg_path,
                   // We mock the SignedExchangeHandler, so just return a JS
                   // content as "application/signed-exchange;v=b3".
                   ResponseEntry("document.title=\"done\";",
                                 "application/signed-exchange;v=b3",
                                 {{"x-content-type-options", "nosniff"}}));
  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue script_header_integrity = {{0x02}};
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

  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(script_sxg_url);

  const auto& cached_exchanges = GetCachedExchanges();
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
}

INSTANTIATE_TEST_SUITE_P(
    SignedExchangeSubresourcePrefetchBrowserTest,
    SignedExchangeSubresourcePrefetchBrowserTest,
    testing::Values(SignedExchangeSubresourcePrefetchBrowserTestParam(false),
                    SignedExchangeSubresourcePrefetchBrowserTestParam(true)));
}  // namespace content
