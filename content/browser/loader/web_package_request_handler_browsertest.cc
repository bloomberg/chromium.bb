// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "services/network/public/cpp/features.h"

namespace content {

class WebPackageRequestHandlerBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  WebPackageRequestHandlerBrowserTest() = default;
  ~WebPackageRequestHandlerBrowserTest() = default;

  void SetUp() override {
    if (is_network_service_enabled()) {
      feature_list_.InitWithFeatures(
          {features::kSignedHTTPExchange, network::features::kNetworkService},
          {});
    } else {
      feature_list_.InitWithFeatures({features::kSignedHTTPExchange}, {});
    }
    ContentBrowserTest::SetUp();
  }

 private:
  bool is_network_service_enabled() const { return GetParam(); }

  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WebPackageRequestHandlerBrowserTest);
};

IN_PROC_BROWSER_TEST_P(WebPackageRequestHandlerBrowserTest, Simple) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/htxg/origin-signed-response.html");
  base::string16 title = base::ASCIIToUTF16("https://example.com/test.html");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  NavigateToURL(shell(), url);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
}

INSTANTIATE_TEST_CASE_P(WebPackageRequestHandlerBrowserTest,
                        WebPackageRequestHandlerBrowserTest,
                        testing::Bool());

}  // namespace content
