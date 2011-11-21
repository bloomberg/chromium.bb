// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/html_dialog_view.h"
#include "chrome/browser/ui/webui/test_html_dialog_ui_delegate.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/widget/widget.h"

using testing::Eq;

namespace {

// Initial size of HTMLDialog for SizeWindow test case.
const int kInitialWidth = 40;
const int kInitialHeight = 40;

class TestHtmlDialogView: public HtmlDialogView {
 public:
  TestHtmlDialogView(Profile* profile, HtmlDialogUIDelegate* delegate)
      : HtmlDialogView(profile, delegate),
        painted_(false),
        should_quit_on_size_change_(false) {
    delegate->GetDialogSize(&last_size_);
  }

  bool painted() const {
    return painted_;
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
      MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
          &MessageLoop::Quit, base::Unretained(MessageLoop::current())));
    }

    last_size_ = bounds.size();
  }

  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE {
    should_quit_on_size_change_ = false;  // No quit when we are closing.
    HtmlDialogView::OnDialogClosed(json_retval);
  }

  virtual void OnTabMainFrameFirstRender() OVERRIDE {
    HtmlDialogView::OnTabMainFrameFirstRender();
    painted_ = true;
    MessageLoop::current()->Quit();
  }

  // Whether first rendered notification is received.
  bool painted_;

  // Whether we should quit message loop when size change is detected.
  bool should_quit_on_size_change_;
  gfx::Size last_size_;

  DISALLOW_COPY_AND_ASSIGN(TestHtmlDialogView);
};

}  // namespace

class HtmlDialogBrowserTest : public InProcessBrowserTest {
 public:
  HtmlDialogBrowserTest() {}
};

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_SizeWindow SizeWindow
#else
// http://code.google.com/p/chromium/issues/detail?id=52602
// Windows has some issues resizing windows- an off by one problem,
// and a minimum size that seems too big.  This file isn't included in
// Mac builds yet. On Chrome OS, this test doesn't apply since ChromeOS
// doesn't allow resizing of windows.
#define MAYBE_SizeWindow DISABLED_SizeWindow
#endif

IN_PROC_BROWSER_TEST_F(HtmlDialogBrowserTest, MAYBE_SizeWindow) {
  test::TestHtmlDialogUIDelegate* delegate = new test::TestHtmlDialogUIDelegate(
      GURL(chrome::kChromeUIChromeURLsURL));
  delegate->set_size(kInitialWidth, kInitialHeight);

  TestHtmlDialogView* html_view =
      new TestHtmlDialogView(browser()->profile(), delegate);
  TabContents* tab_contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(tab_contents != NULL);
  views::Widget::CreateWindowWithParent(html_view,
                                        tab_contents->GetDialogRootWindow());
  html_view->InitDialog();
  html_view->GetWidget()->Show();

  // TestHtmlDialogView should quit current message loop on size change.
  html_view->set_should_quit_on_size_change(true);

  gfx::Rect bounds = html_view->GetWidget()->GetClientAreaScreenBounds();

  gfx::Rect set_bounds = bounds;
  gfx::Rect actual_bounds, rwhv_bounds;

  // Bigger than the default in both dimensions.
  set_bounds.set_width(400);
  set_bounds.set_height(300);

  html_view->MoveContents(tab_contents, set_bounds);
  ui_test_utils::RunMessageLoop();  // TestHtmlDialogView will quit.
  actual_bounds = html_view->GetWidget()->GetClientAreaScreenBounds();
  EXPECT_EQ(set_bounds, actual_bounds);

  rwhv_bounds = html_view->dom_contents()->tab_contents()->
      GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Larger in one dimension and smaller in the other.
  set_bounds.set_width(550);
  set_bounds.set_height(250);

  html_view->MoveContents(tab_contents, set_bounds);
  ui_test_utils::RunMessageLoop();  // TestHtmlDialogView will quit.
  actual_bounds = html_view->GetWidget()->GetClientAreaScreenBounds();
  EXPECT_EQ(set_bounds, actual_bounds);

  rwhv_bounds = html_view->dom_contents()->tab_contents()->
      GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Get very small.
  gfx::Size min_size = html_view->GetWidget()->GetMinimumSize();
  set_bounds.set_size(min_size);

  html_view->MoveContents(tab_contents, set_bounds);
  ui_test_utils::RunMessageLoop();  // TestHtmlDialogView will quit.
  actual_bounds = html_view->GetWidget()->GetClientAreaScreenBounds();
  EXPECT_EQ(set_bounds, actual_bounds);

  rwhv_bounds = html_view->dom_contents()->tab_contents()->
      GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Check to make sure we can't get to 0x0
  set_bounds.set_width(0);
  set_bounds.set_height(0);

  html_view->MoveContents(tab_contents, set_bounds);
  ui_test_utils::RunMessageLoop();  // TestHtmlDialogView will quit.
  actual_bounds = html_view->GetWidget()->GetClientAreaScreenBounds();
  EXPECT_LT(0, actual_bounds.width());
  EXPECT_LT(0, actual_bounds.height());
}

// This is timing out about 5~10% of runs. See crbug.com/86059.
IN_PROC_BROWSER_TEST_F(HtmlDialogBrowserTest, DISABLED_WebContentRendered) {
  HtmlDialogUIDelegate* delegate = new test::TestHtmlDialogUIDelegate(
      GURL(chrome::kChromeUIChromeURLsURL));

  TestHtmlDialogView* html_view =
      new TestHtmlDialogView(browser()->profile(), delegate);
  TabContents* tab_contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(tab_contents != NULL);
  views::Widget::CreateWindowWithParent(html_view,
                                        tab_contents->GetDialogRootWindow());
  EXPECT_TRUE(html_view->initialized_);

  html_view->InitDialog();
  html_view->GetWidget()->Show();

  // TestHtmlDialogView::OnTabMainFrameFirstRender() will Quit().
  MessageLoopForUI::current()->Run();

  EXPECT_TRUE(html_view->painted());

  html_view->GetWidget()->Close();
}
