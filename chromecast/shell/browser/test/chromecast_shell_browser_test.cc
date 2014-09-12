// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chromecast/shell/browser/test/chromecast_browser_test.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace chromecast {
namespace shell {

class ChromecastShellBrowserTest : public ChromecastBrowserTest {
 public:
  ChromecastShellBrowserTest() : url_(url::kAboutBlankURL) {}

  virtual void SetUpOnMainThread() OVERRIDE {
    CreateBrowser();
    NavigateToURL(web_contents(), url_);
  }

 private:
  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(ChromecastShellBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ChromecastShellBrowserTest, EmptyTest) {
  // Run an entire browser lifecycle to ensure nothing breaks.
  // TODO(gunsch): Remove this test case once there are actual assertions to
  // test in a ChromecastBrowserTest instance.
  EXPECT_TRUE(true);
}

}  // namespace shell
}  // namespace chromecast
