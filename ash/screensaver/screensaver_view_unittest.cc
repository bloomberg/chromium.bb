// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screensaver/screensaver_view.h"

#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/webview_test_helper.h"

namespace ash {
namespace test {

class ScreensaverViewTest : public ash::test::AshTestBase {
 public:
  ScreensaverViewTest() {
    url_ = GURL("http://www.google.com");
    views_delegate_.reset(new views::TestViewsDelegate);
    webview_test_helper_.reset(new views::WebViewTestHelper(message_loop()));
  }

  virtual ~ScreensaverViewTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    RunAllPendingInMessageLoop();
  }

  virtual void TearDown() OVERRIDE {
    AshTestBase::TearDown();
  }

  void ExpectOpenScreensaver() {
    internal::ScreensaverView* screensaver =
        internal::ScreensaverView::GetInstance();
    EXPECT_TRUE(screensaver != NULL);
    if (!screensaver) return;

    EXPECT_TRUE(screensaver->screensaver_webview_ != NULL);
    if (!screensaver->screensaver_webview_) return;

    EXPECT_TRUE(screensaver->screensaver_webview_->web_contents() != NULL);
    if (!screensaver->screensaver_webview_->web_contents()) return;

    EXPECT_EQ(screensaver->screensaver_webview_->web_contents()->GetURL(),
              url_);
  }

  void ExpectClosedScreensaver() {
    EXPECT_TRUE(internal::ScreensaverView::GetInstance() == NULL);
  }

 protected:
  GURL url_;

 private:
  scoped_ptr<views::TestViewsDelegate> views_delegate_;
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
