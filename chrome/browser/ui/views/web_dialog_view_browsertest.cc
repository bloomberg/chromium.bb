// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/test/test_web_dialog_delegate.h"

using content::BrowserContext;
using content::WebContents;
using testing::Eq;
using ui::WebDialogDelegate;

namespace {

// Initial size of WebDialog for SizeWindow test case.
const int kInitialWidth = 40;
const int kInitialHeight = 40;

class TestWebDialogView : public views::WebDialogView {
 public:
  TestWebDialogView(content::BrowserContext* context,
                    WebDialogDelegate* delegate)
      : views::WebDialogView(context, delegate, new ChromeWebContentsHandler),
        should_quit_on_size_change_(false) {
    delegate->GetDialogSize(&last_size_);
  }

  void set_should_quit_on_size_change(bool should_quit) {
    should_quit_on_size_change_ = should_quit;
  }

 private:
  // TODO(xiyuan): Update this when WidgetDelegate has bounds change hook.
  virtual void SaveWindowPlacement(const gfx::Rect& bounds,
                                   ui::WindowShowState show_state) OVERRIDE {
    if (should_quit_on_size_change_ && last_size_ != bounds.size()) {
      // Schedule message loop quit because we could be called while
      // the bounds change call is on the stack and not in the nested message
      // loop.
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&base::MessageLoop::Quit,
                     base::Unretained(base::MessageLoop::current())));
    }

    last_size_ = bounds.size();
  }

  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE {
    should_quit_on_size_change_ = false;  // No quit when we are closing.
    views::WebDialogView::OnDialogClosed(json_retval);
  }

  // Whether we should quit message loop when size change is detected.
  bool should_quit_on_size_change_;
  gfx::Size last_size_;

  DISALLOW_COPY_AND_ASSIGN(TestWebDialogView);
};

}  // namespace

class WebDialogBrowserTest : public InProcessBrowserTest {
 public:
  WebDialogBrowserTest() {}
};

// http://code.google.com/p/chromium/issues/detail?id=52602
// Windows has some issues resizing windows- an off by one problem,
// and a minimum size that seems too big.  This file isn't included in
// Mac builds yet. On Chrome OS, this test doesn't apply since ChromeOS
// doesn't allow resizing of windows.
IN_PROC_BROWSER_TEST_F(WebDialogBrowserTest, DISABLED_SizeWindow) {
  ui::test::TestWebDialogDelegate* delegate =
      new ui::test::TestWebDialogDelegate(
          GURL(chrome::kChromeUIChromeURLsURL));
  delegate->set_size(kInitialWidth, kInitialHeight);

  TestWebDialogView* view =
      new TestWebDialogView(browser()->profile(), delegate);
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents != NULL);
  views::Widget::CreateWindowWithParent(
      view, web_contents->GetTopLevelNativeWindow());
  view->GetWidget()->Show();

  // TestWebDialogView should quit current message loop on size change.
  view->set_should_quit_on_size_change(true);

  gfx::Rect bounds = view->GetWidget()->GetClientAreaBoundsInScreen();

  gfx::Rect set_bounds = bounds;
  gfx::Rect actual_bounds, rwhv_bounds;

  // Bigger than the default in both dimensions.
  set_bounds.set_width(400);
  set_bounds.set_height(300);

  view->MoveContents(web_contents, set_bounds);
  content::RunMessageLoop();  // TestWebDialogView will quit.
  actual_bounds = view->GetWidget()->GetClientAreaBoundsInScreen();
  EXPECT_EQ(set_bounds, actual_bounds);

  rwhv_bounds =
      view->web_contents()->GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Larger in one dimension and smaller in the other.
  set_bounds.set_width(550);
  set_bounds.set_height(250);

  view->MoveContents(web_contents, set_bounds);
  content::RunMessageLoop();  // TestWebDialogView will quit.
  actual_bounds = view->GetWidget()->GetClientAreaBoundsInScreen();
  EXPECT_EQ(set_bounds, actual_bounds);

  rwhv_bounds =
      view->web_contents()->GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Get very small.
  gfx::Size min_size = view->GetWidget()->GetMinimumSize();
  set_bounds.set_size(min_size);

  view->MoveContents(web_contents, set_bounds);
  content::RunMessageLoop();  // TestWebDialogView will quit.
  actual_bounds = view->GetWidget()->GetClientAreaBoundsInScreen();
  EXPECT_EQ(set_bounds, actual_bounds);

  rwhv_bounds =
      view->web_contents()->GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Check to make sure we can't get to 0x0
  set_bounds.set_width(0);
  set_bounds.set_height(0);

  view->MoveContents(web_contents, set_bounds);
  content::RunMessageLoop();  // TestWebDialogView will quit.
  actual_bounds = view->GetWidget()->GetClientAreaBoundsInScreen();
  EXPECT_LT(0, actual_bounds.width());
  EXPECT_LT(0, actual_bounds.height());
}
