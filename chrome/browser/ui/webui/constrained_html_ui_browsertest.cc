// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/file_path.h"
#include "base/message_loop.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/constrained_html_ui.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;

namespace {

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
    return GURL(chrome::kChromeUIConstrainedHTMLTestURL);
  }
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const {}
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
  virtual bool ShouldShowDialogTitle() const { return true; }
};

}  // namespace

class ConstrainedHtmlDialogBrowserTest : public InProcessBrowserTest {
 public:
  ConstrainedHtmlDialogBrowserTest() {}
};

// Tests that opening/closing the constrained window won't crash it.
IN_PROC_BROWSER_TEST_F(ConstrainedHtmlDialogBrowserTest, BasicTest) {
  // The delegate deletes itself.
  HtmlDialogUIDelegate* delegate = new TestHtmlDialogUIDelegate();
  TabContents* tab_contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(tab_contents != NULL);

  ConstrainedHtmlUI::CreateConstrainedHtmlDialog(browser()->profile(),
                                                 delegate,
                                                 tab_contents);

  ASSERT_EQ(1U, tab_contents->constrained_window_count());
}
