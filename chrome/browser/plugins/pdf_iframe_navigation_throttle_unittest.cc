// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/http/http_util.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "content/public/browser/plugin_service.h"
#endif

namespace {

const char kHeader[] = "HTTP/1.1 200 OK\r\n";
const char kExampleURL[] = "http://example.com";

#if BUILDFLAG(ENABLE_PLUGINS)
void PluginsLoadedCallback(base::OnceClosure callback,
                           const std::vector<content::WebPluginInfo>& plugins) {
  std::move(callback).Run();
}
#endif

}  // namespace

class PDFIFrameNavigationThrottleTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetAlwaysOpenPdfExternallyForTests(bool always_open_pdf_externally) {
#if BUILDFLAG(ENABLE_PLUGINS)
    PluginPrefs::GetForTestingProfile(profile())
        ->SetAlwaysOpenPdfExternallyForTests(always_open_pdf_externally);
    ChromePluginServiceFilter* filter =
        ChromePluginServiceFilter::GetInstance();
    filter->RegisterResourceContext(profile(), profile()->GetResourceContext());
#endif
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

#if BUILDFLAG(ENABLE_PLUGINS)
    content::PluginService::GetInstance()->Init();

    // Load plugins.
    base::RunLoop run_loop;
    content::PluginService::GetInstance()->GetPlugins(
        base::BindOnce(&PluginsLoadedCallback, run_loop.QuitClosure()));
    run_loop.Run();
#endif

    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    subframe_ = content::RenderFrameHostTester::For(main_rfh())
                    ->AppendChild("subframe");

    feature_list_.InitAndEnableFeature(features::kClickToOpenPDFPlaceholder);
  }

  base::test::ScopedFeatureList feature_list_;
  content::RenderFrameHost* subframe_;
};

TEST_F(PDFIFrameNavigationThrottleTest, OnlyCreateThrottleForSubframes) {
  // Disable the PDF plugin to test main vs. subframes.
  SetAlwaysOpenPdfExternallyForTests(true);

  // Never create throttle for main frames.
  std::string raw_response_headers =
      net::HttpUtil::AssembleRawHeaders(kHeader, strlen(kHeader));
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders(raw_response_headers);
  content::MockNavigationHandle handle(GURL(kExampleURL), main_rfh());
  handle.set_response_headers(headers.get());
  std::unique_ptr<content::NavigationThrottle> throttle =
      PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(&handle);
  ASSERT_EQ(nullptr, throttle);

  // Create a throttle for subframes.
  handle.set_render_frame_host(subframe());
  throttle = PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(&handle);
  ASSERT_NE(nullptr, throttle);
}

TEST_F(PDFIFrameNavigationThrottleTest, InterceptPDFOnly) {
  // Setup
  SetAlwaysOpenPdfExternallyForTests(true);

  std::string raw_response_headers = GetHeaderWithMimeType("application/pdf");
  raw_response_headers = net::HttpUtil::AssembleRawHeaders(
      raw_response_headers.c_str(), raw_response_headers.size());
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders(raw_response_headers);
  content::MockNavigationHandle handle(GURL(kExampleURL), subframe());
  handle.set_response_headers(headers.get());

  // Verify that we CANCEL for PDF mime type.
  std::unique_ptr<content::NavigationThrottle> throttle =
      PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(&handle);

  ASSERT_NE(nullptr, throttle);
  ASSERT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillProcessResponse().action());

  // Verify that we PROCEED for other mime types.
  // Blank mime type
  raw_response_headers =
      net::HttpUtil::AssembleRawHeaders(kHeader, strlen(kHeader));
  headers = new net::HttpResponseHeaders(raw_response_headers);
  handle.set_response_headers(headers.get());
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse().action());

  // HTML
  raw_response_headers = GetHeaderWithMimeType("text/html");
  headers = new net::HttpResponseHeaders(raw_response_headers);
  handle.set_response_headers(headers.get());
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse().action());

  // PNG
  raw_response_headers = GetHeaderWithMimeType("image/png");
  headers = new net::HttpResponseHeaders(raw_response_headers);
  handle.set_response_headers(headers.get());
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse().action());
}

TEST_F(PDFIFrameNavigationThrottleTest, AllowPDFAttachments) {
  // Setup
  SetAlwaysOpenPdfExternallyForTests(true);

  // Verify that we PROCEED for PDF mime types with an attachment
  // content-disposition.
  std::string raw_response_headers =
      "HTTP/1.1 200 OK\r\n"
      "content-type: application/pdf\r\n"
      "content-disposition: attachment\r\n";
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders(raw_response_headers);
  content::MockNavigationHandle handle(GURL(kExampleURL), subframe());
  handle.set_response_headers(headers.get());
  std::unique_ptr<content::NavigationThrottle> throttle =
      PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(&handle);

  ASSERT_NE(nullptr, throttle);
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse().action());
}

#if BUILDFLAG(ENABLE_PLUGINS)
TEST_F(PDFIFrameNavigationThrottleTest, CancelOnlyIfPDFViewerIsDisabled) {
  // Setup
  std::string raw_response_headers = GetHeaderWithMimeType("application/pdf");
  raw_response_headers = net::HttpUtil::AssembleRawHeaders(
      raw_response_headers.c_str(), raw_response_headers.size());
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders(raw_response_headers);
  content::MockNavigationHandle handle(GURL(kExampleURL), subframe());
  handle.set_response_headers(headers.get());

  // Test PDF Viewer enabled.
  SetAlwaysOpenPdfExternallyForTests(false);
  std::unique_ptr<content::NavigationThrottle> throttle =
      PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(&handle);
  ASSERT_EQ(nullptr, throttle);

  // Test PDF Viewer disabled.
  SetAlwaysOpenPdfExternallyForTests(true);
  throttle = PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(&handle);

  ASSERT_NE(nullptr, throttle);
  ASSERT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillProcessResponse().action());
}
#endif
