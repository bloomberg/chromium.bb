// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace headless {

class HeadlessWebContentsTest : public HeadlessBrowserTest {};

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, Navigation) {
  EXPECT_TRUE(embedded_test_server()->Start());
  scoped_ptr<HeadlessWebContents> web_contents =
      browser()->CreateWebContents(gfx::Size(800, 600));
  EXPECT_TRUE(NavigateAndWaitForLoad(
      web_contents.get(), embedded_test_server()->GetURL("/hello.html")));
}

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, NavigationWithBadURL) {
  scoped_ptr<HeadlessWebContents> web_contents =
      browser()->CreateWebContents(gfx::Size(800, 600));
  GURL bad_url("not_valid");
  EXPECT_FALSE(web_contents->OpenURL(bad_url));
}

}  // namespace headless
