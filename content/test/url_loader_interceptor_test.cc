// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/url_loader_interceptor.h"
#include "base/command_line.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {
namespace {

class URLLoaderInterceptorTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void Test() { EXPECT_TRUE(NavigateToURL(shell(), GetPageURL())); }

  GURL GetPageURL() {
    return embedded_test_server()->GetURL("/page_with_image.html");
  }

  bool DidImageLoad() {
    int height = 0;
    EXPECT_TRUE(ExecuteScriptAndExtractInt(
        shell(),
        "window.domAutomationController.send("
        "document.getElementsByTagName('img')[0].naturalHeight)",
        &height));
    return !!height;
  }

  GURL GetImageURL() { return embedded_test_server()->GetURL("/blank.jpg"); }

  StoragePartition* GetStoragePartition() {
    return shell()
        ->web_contents()
        ->GetMainFrame()
        ->GetProcess()
        ->GetStoragePartition();
  }
};

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, NoInterception) {
  URLLoaderInterceptor interceptor(
      base::Bind([](URLLoaderInterceptor::RequestParams* params) {
        NOTREACHED();
        return false;
      }),
      GetStoragePartition(), false, false);
  Test();
}

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, MonitorFrame) {
  if (!base::FeatureList::IsEnabled(features::kNetworkService))
    return;  // Depends on http://crbug.com/747130.

  bool seen = false;
  URLLoaderInterceptor interceptor(
      base::Bind(
          [](bool* seen, const GURL& url,
             URLLoaderInterceptor::RequestParams* params) {
            EXPECT_EQ(params->url_request.url, url);
            EXPECT_EQ(params->process_id, 0);
            EXPECT_FALSE(*seen);
            *seen = true;
            return false;
          },
          &seen, GetPageURL()),
      GetStoragePartition(), true, false);
  Test();
  EXPECT_TRUE(seen);
}

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, InterceptFrame) {
  if (!base::FeatureList::IsEnabled(features::kNetworkService))
    return;  // Depends on http://crbug.com/747130.

  URLLoaderInterceptor interceptor(
      base::Bind(
          [](const GURL& url, URLLoaderInterceptor::RequestParams* params) {
            EXPECT_EQ(params->url_request.url, url);
            EXPECT_EQ(params->process_id, 0);
            network::URLLoaderCompletionStatus status;
            status.error_code = net::ERR_FAILED;
            params->client->OnComplete(status);
            return true;
          },
          GetPageURL()),
      GetStoragePartition(), true, false);
  EXPECT_FALSE(NavigateToURL(shell(), GetPageURL()));
}

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, MonitorSubresource) {
  if (!IsBrowserSideNavigationEnabled())
    return;  // Very deprecated non-plznavigate code path not supported.

  bool seen = false;
  URLLoaderInterceptor interceptor(
      base::Bind(
          [](bool* seen, const GURL& url,
             URLLoaderInterceptor::RequestParams* params) {
            EXPECT_EQ(params->url_request.url, url);
            EXPECT_NE(params->process_id, 0);
            EXPECT_FALSE(*seen);
            *seen = true;
            return false;
          },
          &seen, GetImageURL()),
      GetStoragePartition(), false, true);
  Test();
  EXPECT_TRUE(seen);
  EXPECT_TRUE(DidImageLoad());
}

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, InterceptSubresource) {
  if (!IsBrowserSideNavigationEnabled())
    return;  // Very deprecated non-plznavigate code path not supported.

  URLLoaderInterceptor interceptor(
      base::Bind(
          [](const GURL& url, URLLoaderInterceptor::RequestParams* params) {
            EXPECT_EQ(params->url_request.url, url);
            network::URLLoaderCompletionStatus status;
            status.error_code = net::ERR_FAILED;
            params->client->OnComplete(status);
            return true;
          },
          GetImageURL()),
      GetStoragePartition(), false, true);
  Test();
  EXPECT_FALSE(DidImageLoad());
}

}  // namespace
}  // namespace content
