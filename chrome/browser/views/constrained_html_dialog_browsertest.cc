// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)

#include "chrome/test/ui/ui_test.h"

#include "base/file_path.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/constrained_html_dialog.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/constrained_html_dialog_win.h"
#include "chrome/browser/views/html_dialog_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
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
    return GURL(chrome::kAboutAboutURL);
  }
  virtual void GetDOMMessageHandlers(
      std::vector<DOMMessageHandler*>* handlers) const {}
  virtual void GetDialogSize(gfx::Size* size) const {
    size->set_width(400);
    size->set_height(400);
  }
  virtual std::string GetDialogArgs() const {
    return std::string();
  }
  virtual void OnDialogClosed(const std::string& json_retval) { }
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog) {
    if (out_close_dialog)
      *out_close_dialog = true;
  }
};

}  // namespace

class ConstrainedHtmlDialogBrowserTest : public InProcessBrowserTest {
 public:
  ConstrainedHtmlDialogBrowserTest() {}
};

IN_PROC_BROWSER_TEST_F(ConstrainedHtmlDialogBrowserTest, LoadWindow) {
  HtmlDialogUIDelegate* delegate = new TestHtmlDialogUIDelegate();

  ConstrainedHtmlDialogWin* dialog =
    new ConstrainedHtmlDialogWin(browser()->profile(), delegate);

  TabContents* tab_contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(tab_contents != NULL);
  ConstrainedWindow* window = tab_contents->CreateConstrainedDialog(
      dialog->GetConstrainedWindowDelegate());

  EXPECT_EQ(1, tab_contents->constrained_window_count());

  gfx::Rect bounds = dialog->GetContentsView()->bounds();
  EXPECT_LT(0, bounds.width());
  EXPECT_LT(0, bounds.height());
  EXPECT_GE(400, bounds.width());
  EXPECT_GE(400, bounds.height());
}

#endif  // OS_WIN
