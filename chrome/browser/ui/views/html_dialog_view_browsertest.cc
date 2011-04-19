// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/file_path.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/views/html_dialog_view.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

using testing::Eq;

namespace {

// Window non-client-area means that the minimum size for the window
// won't be the actual minimum size - our layout and resizing code
// makes sure the chrome is always visible.
const int kMinimumWidthToTestFor = 20;
const int kMinimumHeightToTestFor = 30;

class TestHtmlDialogUIDelegate : public HtmlDialogUIDelegate {
 public:
  TestHtmlDialogUIDelegate() {}
  virtual ~TestHtmlDialogUIDelegate() {}

  // HTMLDialogUIDelegate implementation:
  virtual bool IsDialogModal() const {
    return true;
  }
  virtual std::wstring GetDialogTitle() const {
    return std::wstring(L"Test");
  }
  virtual GURL GetDialogContentURL() const {
    return GURL(chrome::kAboutBlankURL);
  }
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const { }
  virtual void GetDialogSize(gfx::Size* size) const {
    size->set_width(40);
    size->set_height(40);
  }
  virtual std::string GetDialogArgs() const {
    return std::string();
  }
  virtual void OnDialogClosed(const std::string& json_retval) { }
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog) {
    if (out_close_dialog)
      *out_close_dialog = true;
  }
  virtual bool ShouldShowDialogTitle() const { return true; }
};

}  // namespace

class HtmlDialogBrowserTest : public InProcessBrowserTest {
 public:
  HtmlDialogBrowserTest() {}

#if defined(OS_WIN)
  class WindowChangedObserver : public base::MessagePumpForUI::Observer {
   public:
    WindowChangedObserver() {}

    static WindowChangedObserver* GetInstance() {
      return Singleton<WindowChangedObserver>::get();
    }

    // This method is called before processing a message.
    virtual void WillProcessMessage(const MSG& msg) {}

    // This method is called after processing a message.
    virtual void DidProcessMessage(const MSG& msg) {
      // Either WM_PAINT or WM_TIMER indicates the actual work of
      // pushing through the window resizing messages is done since
      // they are lower priority (we don't get to see the
      // WM_WINDOWPOSCHANGED message here).
      if (msg.message == WM_PAINT || msg.message == WM_TIMER)
        MessageLoop::current()->Quit();
    }
  };
#elif !defined(OS_MACOSX)
  class WindowChangedObserver : public base::MessagePumpForUI::Observer {
   public:
    WindowChangedObserver() {}

    static WindowChangedObserver* GetInstance() {
      return Singleton<WindowChangedObserver>::get();
    }

    // This method is called before processing a message.
    virtual void WillProcessEvent(GdkEvent* event) {}

    // This method is called after processing a message.
    virtual void DidProcessEvent(GdkEvent* event) {
      // Quit once the GDK_CONFIGURE event has been processed - seeing
      // this means the window sizing request that was made actually
      // happened.
      if (event->type == GDK_CONFIGURE)
        MessageLoop::current()->Quit();
    }
  };
#endif
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
  HtmlDialogUIDelegate* delegate = new TestHtmlDialogUIDelegate();

  HtmlDialogView* html_view =
      new HtmlDialogView(browser()->profile(), delegate);
  TabContents* tab_contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(tab_contents != NULL);
  views::Window::CreateChromeWindow(tab_contents->GetMessageBoxRootWindow(),
                                    gfx::Rect(), html_view);
  html_view->InitDialog();
  html_view->window()->Show();

  MessageLoopForUI::current()->AddObserver(
      WindowChangedObserver::GetInstance());

  gfx::Rect bounds = html_view->GetWidget()->GetClientAreaScreenBounds();

  gfx::Rect set_bounds = bounds;
  gfx::Rect actual_bounds, rwhv_bounds;

  // Bigger than the default in both dimensions.
  set_bounds.set_width(400);
  set_bounds.set_height(300);

  html_view->MoveContents(tab_contents, set_bounds);
  ui_test_utils::RunMessageLoop();
  actual_bounds = html_view->GetWidget()->GetClientAreaScreenBounds();
  EXPECT_EQ(set_bounds, actual_bounds);

  rwhv_bounds =
      html_view->tab_contents()->GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Larger in one dimension and smaller in the other.
  set_bounds.set_width(550);
  set_bounds.set_height(250);

  html_view->MoveContents(tab_contents, set_bounds);
  ui_test_utils::RunMessageLoop();
  actual_bounds = html_view->GetWidget()->GetClientAreaScreenBounds();
  EXPECT_EQ(set_bounds, actual_bounds);

  rwhv_bounds =
      html_view->tab_contents()->GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Get very small.
  set_bounds.set_width(kMinimumWidthToTestFor);
  set_bounds.set_height(kMinimumHeightToTestFor);

  html_view->MoveContents(tab_contents, set_bounds);
  ui_test_utils::RunMessageLoop();
  actual_bounds = html_view->GetWidget()->GetClientAreaScreenBounds();
  EXPECT_EQ(set_bounds, actual_bounds);

  rwhv_bounds =
      html_view->tab_contents()->GetRenderWidgetHostView()->GetViewBounds();
  EXPECT_LT(0, rwhv_bounds.width());
  EXPECT_LT(0, rwhv_bounds.height());
  EXPECT_GE(set_bounds.width(), rwhv_bounds.width());
  EXPECT_GE(set_bounds.height(), rwhv_bounds.height());

  // Check to make sure we can't get to 0x0
  set_bounds.set_width(0);
  set_bounds.set_height(0);

  html_view->MoveContents(tab_contents, set_bounds);
  ui_test_utils::RunMessageLoop();
  actual_bounds = html_view->GetWidget()->GetClientAreaScreenBounds();
  EXPECT_LT(0, actual_bounds.width());
  EXPECT_LT(0, actual_bounds.height());

  MessageLoopForUI::current()->RemoveObserver(
      WindowChangedObserver::GetInstance());
}
