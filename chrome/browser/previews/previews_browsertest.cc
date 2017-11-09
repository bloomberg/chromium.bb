// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/previews/core/previews_features.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"

class PreviewsBrowserTest : public InProcessBrowserTest {
 public:
  PreviewsBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        noscript_css_requested_(false),
        noscript_js_requested_(false) {
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data/previews");

    https_server_.RegisterRequestMonitor(base::Bind(
        &PreviewsBrowserTest::MonitorResourceRequest, base::Unretained(this)));

    EXPECT_TRUE(https_server_.Start());

    test_url_ = https_server_.GetURL("/noscript_test.html");
    EXPECT_TRUE(test_url_.SchemeIsHTTPOrHTTPS());
    EXPECT_TRUE(test_url_.SchemeIsCryptographic());
  }

  ~PreviewsBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    cmd->AppendSwitchASCII("force-effective-connection-type", "Slow-2G");
  }

  const GURL& test_url() const { return test_url_; }

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

  net::EmbeddedTestServer https_server_;
  GURL test_url_;
  bool noscript_css_requested_;
  bool noscript_js_requested_;
};

// Loads a webpage that has both script and noscript tags and also requests
// a script resource. Verifies that the noscript tag is not evaluated and the
// script resource is loaded.
IN_PROC_BROWSER_TEST_F(PreviewsBrowserTest, NoScriptPreviewsDisabled) {
  ui_test_utils::NavigateToURL(browser(), test_url());

  // Verify loaded js resource but not css triggered by noscript tag.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());
}

// This test class enables NoScriptPreviews via command line addition.
class PreviewsNoScriptBrowserTest : public PreviewsBrowserTest {
 public:
  PreviewsNoScriptBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        previews::features::kNoScriptPreviews);
  }

  ~PreviewsNoScriptBrowserTest() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Previews InfoBar (which this test triggers) does not work on Mac.
// See crbug.com/782322 for detail.
#if defined(OS_MACOSX)
#define MAYBE_NoScriptPreviewsEnabled DISABLED_NoScriptPreviewsEnabled
#else
#define MAYBE_NoScriptPreviewsEnabled NoScriptPreviewsEnabled
#endif

// Loads a webpage that has both script and noscript tags and also requests
// a script resource. Verifies that the noscript tag is evaluated and the
// script resource is not loaded.
IN_PROC_BROWSER_TEST_F(PreviewsNoScriptBrowserTest,
                       MAYBE_NoScriptPreviewsEnabled) {
  ui_test_utils::NavigateToURL(browser(), test_url());

  // Verify loaded noscript tag triggered css resource but not js one.
  EXPECT_TRUE(noscript_css_requested());
  EXPECT_FALSE(noscript_js_requested());
}
