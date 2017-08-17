// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"

#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/plugin_service.h"
#include "net/http/http_util.h"

namespace {

const char kHeader[] = "HTTP/1.1 200 OK\r\n";
const char kExampleURL[] = "http://example.com";

void PluginsLoadedCallback(base::OnceClosure callback,
                           const std::vector<content::WebPluginInfo>& plugins) {
  std::move(callback).Run();
}

}  // namespace

class PDFIFrameNavigationThrottleTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetAlwaysOpenPdfExternallyForTests(bool always_open_pdf_externally) {
    PluginPrefs::GetForTestingProfile(profile())
        ->SetAlwaysOpenPdfExternallyForTests(always_open_pdf_externally);
  }

  std::string GetHeaderWithMimeType(const std::string& mime_type) {
    return "HTTP/1.1 200 OK\r\n"
           "content-type: " +
           mime_type + "\r\n";
  }

  content::RenderFrameHost* subframe() { return subframe_; }

 private:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content::PluginService::GetInstance()->Init();

    // Load plugins.
    base::RunLoop run_loop;
    content::PluginService::GetInstance()->GetPlugins(
        base::BindOnce(&PluginsLoadedCallback, run_loop.QuitClosure()));
    run_loop.Run();

    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    subframe_ = content::RenderFrameHostTester::For(main_rfh())
                    ->AppendChild("subframe");
  }

  content::RenderFrameHost* subframe_;
};

TEST_F(PDFIFrameNavigationThrottleTest, OnlyCreateThrottleForSubframes) {
  // Disable the PDF plugin to test main vs. subframes.
  SetAlwaysOpenPdfExternallyForTests(true);
  ChromePluginServiceFilter* filter = ChromePluginServiceFilter::GetInstance();
  filter->RegisterResourceContext(profile(), profile()->GetResourceContext());

  // Never create throttle for main frames.
  std::unique_ptr<content::NavigationHandle> handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(
          GURL(kExampleURL), main_rfh());

  handle->CallWillProcessResponseForTesting(
      main_rfh(), net::HttpUtil::AssembleRawHeaders(kHeader, strlen(kHeader)));

  std::unique_ptr<content::NavigationThrottle> throttle =
      PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());
  ASSERT_EQ(nullptr, throttle);

  // Create a throttle for subframes.
  handle = content::NavigationHandle::CreateNavigationHandleForTesting(
      GURL(kExampleURL), subframe());

  handle->CallWillProcessResponseForTesting(
      subframe(), net::HttpUtil::AssembleRawHeaders(kHeader, strlen(kHeader)));

  throttle = PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());
  ASSERT_NE(nullptr, throttle);
}

TEST_F(PDFIFrameNavigationThrottleTest, InterceptPDFOnly) {
  // Setup
  SetAlwaysOpenPdfExternallyForTests(true);
  ChromePluginServiceFilter* filter = ChromePluginServiceFilter::GetInstance();
  filter->RegisterResourceContext(profile(), profile()->GetResourceContext());

  std::unique_ptr<content::NavigationHandle> handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(
          GURL(kExampleURL), subframe());

  // Verify that we CANCEL for PDF mime type.
  std::string header = GetHeaderWithMimeType("application/pdf");
  handle->CallWillProcessResponseForTesting(
      subframe(),
      net::HttpUtil::AssembleRawHeaders(header.c_str(), header.size()));

  std::unique_ptr<content::NavigationThrottle> throttle =
      PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());

  ASSERT_NE(nullptr, throttle);
  ASSERT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillProcessResponse());

  // Verify that we PROCEED for other mime types.
  // Blank mime type
  handle->CallWillProcessResponseForTesting(
      subframe(), net::HttpUtil::AssembleRawHeaders(kHeader, strlen(kHeader)));

  throttle = PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());

  ASSERT_NE(nullptr, throttle);
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());

  // HTML
  header = GetHeaderWithMimeType("text/html");
  handle->CallWillProcessResponseForTesting(
      subframe(),
      net::HttpUtil::AssembleRawHeaders(header.c_str(), header.size()));

  throttle = PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());

  ASSERT_NE(nullptr, throttle);
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());

  // PNG
  header = GetHeaderWithMimeType("image/png");
  handle->CallWillProcessResponseForTesting(
      subframe(),
      net::HttpUtil::AssembleRawHeaders(header.c_str(), header.size()));

  throttle = PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());

  ASSERT_NE(nullptr, throttle);
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
}

TEST_F(PDFIFrameNavigationThrottleTest, CancelOnlyIfPDFViewerIsDisabled) {
  // Setup
  std::unique_ptr<content::NavigationHandle> handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(
          GURL(kExampleURL), subframe());

  std::string header = GetHeaderWithMimeType("application/pdf");
  handle->CallWillProcessResponseForTesting(
      subframe(),
      net::HttpUtil::AssembleRawHeaders(header.c_str(), header.size()));

  // Test PDF Viewer enabled.
  SetAlwaysOpenPdfExternallyForTests(false);
  ChromePluginServiceFilter* filter = ChromePluginServiceFilter::GetInstance();
  filter->RegisterResourceContext(profile(), profile()->GetResourceContext());

  std::unique_ptr<content::NavigationThrottle> throttle =
      PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());

  ASSERT_EQ(nullptr, throttle);

  // Test PDF Viewer disabled.
  SetAlwaysOpenPdfExternallyForTests(true);
  filter->RegisterResourceContext(profile(), profile()->GetResourceContext());

  throttle = PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());

  ASSERT_NE(nullptr, throttle);
  ASSERT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillProcessResponse());
}
