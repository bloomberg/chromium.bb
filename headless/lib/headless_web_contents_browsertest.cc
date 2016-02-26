// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "content/public/test/browser_test.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace headless {

class HeadlessWebContentsTest : public HeadlessBrowserTest {};

class WaitForNavigationObserver : public HeadlessWebContents::Observer {
 public:
  WaitForNavigationObserver(base::RunLoop* run_loop,
                            HeadlessWebContents* web_contents)
      : run_loop_(run_loop), web_contents_(web_contents) {
    web_contents_->AddObserver(this);
  }

  ~WaitForNavigationObserver() override { web_contents_->RemoveObserver(this); }

  void DocumentOnLoadCompletedInMainFrame() override { run_loop_->Quit(); }

 private:
  base::RunLoop* run_loop_;            // Not owned.
  HeadlessWebContents* web_contents_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(WaitForNavigationObserver);
};

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, Navigation) {
  EXPECT_TRUE(embedded_test_server()->Start());
  scoped_ptr<HeadlessWebContents> web_contents =
      browser()->CreateWebContents(gfx::Size(800, 600));

  base::RunLoop run_loop;
  base::MessageLoop::ScopedNestableTaskAllower nestable_allower(
      base::MessageLoop::current());
  WaitForNavigationObserver observer(&run_loop, web_contents.get());

  web_contents->OpenURL(embedded_test_server()->GetURL("/hello.html"));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, NavigationWithBadURL) {
  scoped_ptr<HeadlessWebContents> web_contents =
      browser()->CreateWebContents(gfx::Size(800, 600));
  GURL bad_url("not_valid");
  EXPECT_FALSE(web_contents->OpenURL(bad_url));
}

}  // namespace headless
