// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"

using content::URLLoaderInterceptor;

namespace {
constexpr char kBaseDataDir[] = "content/test/data/origin_trials/";
}  // namespace

namespace content {

class OriginTrialsBrowserTest : public content::ContentBrowserTest {
 public:
  OriginTrialsBrowserTest() : ContentBrowserTest() {}
  ~OriginTrialsBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [&](URLLoaderInterceptor::RequestParams* params) -> bool {
              URLLoaderInterceptor::WriteResponse(
                  base::StrCat(
                      {kBaseDataDir, params->url_request.url.path_piece()}),
                  params->client.get());
              return true;
            }));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

 private:
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;

  DISALLOW_COPY_AND_ASSIGN(OriginTrialsBrowserTest);
};

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest, Basic) {
  NavigateToURL(shell(), GURL("https://example.test/basic.html"));
  // Ensure we can invoke normalMethod(), which is only available when the
  // Frobulate OT is enabled.
  EXPECT_TRUE(content::ExecJs(shell()->web_contents()->GetMainFrame(),
                              "internals.originTrialsTest().normalMethod();"));
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest, Navigation) {
  NavigateToURL(shell(), GURL("https://example.test/navigation.html"));
  // Ensure we can invoke navigationMethod(), which is only available when the
  // FrobulateNavigation OT is enabled.
  EXPECT_TRUE(
      content::ExecJs(shell()->web_contents()->GetMainFrame(),
                      "internals.originTrialsTest().navigationMethod();"));
}

}  // namespace content
