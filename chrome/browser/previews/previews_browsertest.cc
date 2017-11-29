// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/test_component_creator.h"
#include "components/previews/core/previews_features.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

class PreviewsBrowserTest : public InProcessBrowserTest {
 public:
  PreviewsBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        http_server_(net::EmbeddedTestServer::TYPE_HTTP),
        noscript_css_requested_(false),
        noscript_js_requested_(false) {
    // Set up https server with resource monitor.
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data/previews");
    https_server_.RegisterRequestMonitor(base::Bind(
        &PreviewsBrowserTest::MonitorResourceRequest, base::Unretained(this)));
    EXPECT_TRUE(https_server_.Start());

    https_url_ = https_server_.GetURL("/noscript_test.html");
    EXPECT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    // Set up http server with resource monitor and redirect handler.
    http_server_.ServeFilesFromSourceDirectory("chrome/test/data/previews");
    http_server_.RegisterRequestMonitor(base::Bind(
        &PreviewsBrowserTest::MonitorResourceRequest, base::Unretained(this)));
    http_server_.RegisterRequestHandler(base::Bind(
        &PreviewsBrowserTest::HandleRedirectRequest, base::Unretained(this)));
    EXPECT_TRUE(http_server_.Start());

    http_url_ = http_server_.GetURL("/noscript_test.html");
    EXPECT_TRUE(http_url_.SchemeIs(url::kHttpScheme));

    redirect_url_ = http_server_.GetURL("/redirect.html");
    EXPECT_TRUE(redirect_url_.SchemeIs(url::kHttpScheme));
  }

  ~PreviewsBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    cmd->AppendSwitchASCII("force-effective-connection-type", "Slow-2G");
  }

  const GURL& https_url() const { return https_url_; }
  const GURL& http_url() const { return http_url_; }
  const GURL& redirect_url() const { return redirect_url_; }
  bool noscript_css_requested() const { return noscript_css_requested_; }
  bool noscript_js_requested() const { return noscript_js_requested_; }

 private:
  // Called by |https_server_|.
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    if (request.GetURL().spec().find("noscript_test.css") !=
        std::string::npos) {
      noscript_css_requested_ = true;
    }
    if (request.GetURL().spec().find("noscript_test.js") != std::string::npos) {
      noscript_js_requested_ = true;
    }
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRedirectRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response;
    if (request.GetURL().spec().find("redirect") != std::string::npos) {
      response.reset(new net::test_server::BasicHttpResponse);
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", https_url().spec());
    }
    return std::move(response);
  }

  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer http_server_;
  GURL https_url_;
  GURL http_url_;
  GURL redirect_url_;
  bool noscript_css_requested_;
  bool noscript_js_requested_;
};

// Loads a webpage that has both script and noscript tags and also requests
// a script resource. Verifies that the noscript tag is not evaluated and the
// script resource is loaded.
IN_PROC_BROWSER_TEST_F(PreviewsBrowserTest, NoScriptPreviewsDisabled) {
  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verify loaded js resource but not css triggered by noscript tag.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());
}

// This test class enables NoScriptPreviews but without OptimizationHints.
class PreviewsNoScriptBrowserTest : public PreviewsBrowserTest {
 public:
  PreviewsNoScriptBrowserTest() {
    // Explicitly disable server hints.
    scoped_feature_list_.InitWithFeatures(
        {previews::features::kNoScriptPreviews},
        {previews::features::kOptimizationHints});
  }

  ~PreviewsNoScriptBrowserTest() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Previews InfoBar (which these tests triggers) does not work on Mac.
// See crbug.com/782322 for detail.
#if defined(OS_MACOSX)
#define MAYBE_NoScriptPreviewsEnabled DISABLED_NoScriptPreviewsEnabled
#define MAYBE_NoScriptPreviewsEnabledHttpRedirectToHttps \
  DISABLED_NoScriptPreviewsEnabledHttpRedirectToHttps
#else
// Flaky on win7_chromium_rel_ng. crbug.com/789542
#if defined(OS_WIN)
#define MAYBE_NoScriptPreviewsEnabled DISABLED_NoScriptPreviewsEnabled
#else
#define MAYBE_NoScriptPreviewsEnabled NoScriptPreviewsEnabled
#endif
#define MAYBE_NoScriptPreviewsEnabledHttpRedirectToHttps \
  NoScriptPreviewsEnabledHttpRedirectToHttps
#endif

// Loads a webpage that has both script and noscript tags and also requests
// a script resource. Verifies that the noscript tag is evaluated and the
// script resource is not loaded.
IN_PROC_BROWSER_TEST_F(PreviewsNoScriptBrowserTest,
                       MAYBE_NoScriptPreviewsEnabled) {
  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verify loaded noscript tag triggered css resource but not js one.
  EXPECT_TRUE(noscript_css_requested());
  EXPECT_FALSE(noscript_js_requested());
}

IN_PROC_BROWSER_TEST_F(PreviewsNoScriptBrowserTest,
                       NoScriptPreviewsEnabledButHttpRequest) {
  ui_test_utils::NavigateToURL(browser(), http_url());

  // Verify loaded js resource but not css triggered by noscript tag.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());
}

IN_PROC_BROWSER_TEST_F(PreviewsNoScriptBrowserTest,
                       MAYBE_NoScriptPreviewsEnabledHttpRedirectToHttps) {
  ui_test_utils::NavigateToURL(browser(), redirect_url());

  // Verify loaded noscript tag triggered css resource but not js one.
  EXPECT_TRUE(noscript_css_requested());
  EXPECT_FALSE(noscript_js_requested());
}

// This test class enables NoScriptPreviews with OptimizationHints.
class PreviewsOptimizationGuideBrowserTest : public PreviewsBrowserTest {
 public:
  PreviewsOptimizationGuideBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {previews::features::kOptimizationHints,
         previews::features::kNoScriptPreviews},
        {});
  }

  ~PreviewsOptimizationGuideBrowserTest() override {}

  void SetNoScriptWhitelist(
      std::vector<std::string> whitelisted_noscript_sites) {
    const optimization_guide::ComponentInfo& component_info =
        test_component_creator_.CreateComponentInfoWithNoScriptWhitelist(
            whitelisted_noscript_sites);
    g_browser_process->optimization_guide_service()->ProcessHints(
        component_info);

    // Wait for hints to be processed by PreviewsOptimizationGuide.
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  optimization_guide::testing::TestComponentCreator test_component_creator_;
};

// Previews InfoBar (which this test triggers) does not work on Mac.
// See crbug.com/782322 for detail.
#if defined(OS_MACOSX)
#define MAYBE_NoScriptPreviewsEnabledByWhitelist \
  DISABLED_NoScriptPreviewsEnabledByWhitelist
#else
#define MAYBE_NoScriptPreviewsEnabledByWhitelist \
  NoScriptPreviewsEnabledByWhitelist
#endif

IN_PROC_BROWSER_TEST_F(PreviewsOptimizationGuideBrowserTest,
                       MAYBE_NoScriptPreviewsEnabledByWhitelist) {
  // Whitelist test URL for NoScript.
  SetNoScriptWhitelist({https_url().host()});

  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verify loaded noscript tag triggered css resource but not js one.
  EXPECT_TRUE(noscript_css_requested());
  EXPECT_FALSE(noscript_js_requested());
}

IN_PROC_BROWSER_TEST_F(PreviewsOptimizationGuideBrowserTest,
                       NoScriptPreviewsNotEnabledByWhitelist) {
  // Whitelist random site for NoScript.
  SetNoScriptWhitelist({"foo.com"});

  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verify loaded js resource but not css triggered by noscript tag.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());
}
