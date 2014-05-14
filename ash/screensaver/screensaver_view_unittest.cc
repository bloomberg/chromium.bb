// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screensaver/screensaver_view.h"

#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/webview_test_helper.h"

namespace ash {
namespace test {

class ScreensaverViewTest : public ash::test::AshTestBase {
 public:
  ScreensaverViewTest() {
    url_ = GURL("http://www.google.com");
    webview_test_helper_.reset(new views::WebViewTestHelper());
  }

  virtual ~ScreensaverViewTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    RunAllPendingInMessageLoop();
  }

  void ExpectOpenScreensaver() {
    ScreensaverView* screensaver = ScreensaverView::GetInstance();
    EXPECT_TRUE(screensaver != NULL);
    if (!screensaver) return;
    EXPECT_TRUE(screensaver->IsScreensaverShowingURL(url_));
  }

  void ExpectClosedScreensaver() {
    EXPECT_TRUE(ScreensaverView::GetInstance() == NULL);
  }

 protected:
  GURL url_;

 private:
  scoped_ptr<views::WebViewTestHelper> webview_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(ScreensaverViewTest);
};

TEST_F(ScreensaverViewTest, ShowScreensaverAndClose) {
  ash::ShowScreensaver(url_);
  RunAllPendingInMessageLoop();
  ExpectOpenScreensaver();

  ash::CloseScreensaver();
  ExpectClosedScreensaver();
}

TEST_F(ScreensaverViewTest, OutOfOrderMultipleShowAndClose) {
  ash::CloseScreensaver();
  ExpectClosedScreensaver();

  ash::ShowScreensaver(url_);
  ExpectOpenScreensaver();
  RunAllPendingInMessageLoop();
  ash::ShowScreensaver(url_);
  ExpectOpenScreensaver();
  RunAllPendingInMessageLoop();

  ash::CloseScreensaver();
  ExpectClosedScreensaver();
  ash::CloseScreensaver();
  ExpectClosedScreensaver();
}

}  // namespace test
}  // namespace ash
