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

class NavigationObserver : public HeadlessWebContents::Observer {
 public:
  NavigationObserver(HeadlessWebContentsTest* browser_test)
      : browser_test_(browser_test), navigation_succeeded_(false) {}
  ~NavigationObserver() override {}

  void DocumentOnLoadCompletedInMainFrame() override {
    browser_test_->FinishAsynchronousTest();
  }

  void DidFinishNavigation(bool success) override {
    navigation_succeeded_ = success;
  }

  bool navigation_succeeded() const { return navigation_succeeded_; }

 private:
  HeadlessWebContentsTest* browser_test_;  // Not owned.
  bool navigation_succeeded_;
};

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, Navigation) {
  EXPECT_TRUE(embedded_test_server()->Start());
  HeadlessWebContents* web_contents = browser()->CreateWebContents(
      embedded_test_server()->GetURL("/hello.html"), gfx::Size(800, 600));
  NavigationObserver observer(this);
  web_contents->AddObserver(&observer);

  RunAsynchronousTest();

  EXPECT_TRUE(observer.navigation_succeeded());
  web_contents->RemoveObserver(&observer);

  std::vector<HeadlessWebContents*> all_web_contents =
      browser()->GetAllWebContents();

  EXPECT_EQ(static_cast<size_t>(1), all_web_contents.size());
  EXPECT_EQ(web_contents, all_web_contents[0]);
}

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, WindowOpen) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessWebContents* web_contents = browser()->CreateWebContents(
      embedded_test_server()->GetURL("/window_open.html"), gfx::Size(800, 600));
  NavigationObserver observer(this);
  web_contents->AddObserver(&observer);

  RunAsynchronousTest();

  EXPECT_TRUE(observer.navigation_succeeded());
  web_contents->RemoveObserver(&observer);

  std::vector<HeadlessWebContents*> all_web_contents =
      browser()->GetAllWebContents();

  EXPECT_EQ(static_cast<size_t>(2), all_web_contents.size());
}

}  // namespace headless
