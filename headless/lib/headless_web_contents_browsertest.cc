// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

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
  HeadlessWebContents* web_contents = browser()->CreateWebContents(
      embedded_test_server()->GetURL("/hello.html"), gfx::Size(800, 600));
  EXPECT_TRUE(WaitForLoad(web_contents));

  std::vector<HeadlessWebContents*> all_web_contents =
      browser()->GetAllWebContents();

  EXPECT_EQ(static_cast<size_t>(1), all_web_contents.size());
  EXPECT_EQ(web_contents, all_web_contents[0]);
}

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, WindowOpen) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessWebContents* web_contents = browser()->CreateWebContents(
      embedded_test_server()->GetURL("/window_open.html"), gfx::Size(800, 600));
  EXPECT_TRUE(WaitForLoad(web_contents));

  std::vector<HeadlessWebContents*> all_web_contents =
      browser()->GetAllWebContents();

  EXPECT_EQ(static_cast<size_t>(2), all_web_contents.size());
}

}  // namespace headless
