// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/constrained_html_ui.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_observer.h"

namespace {

class TestHtmlDialogUIDelegate : public HtmlDialogUIDelegate {
 public:
  TestHtmlDialogUIDelegate() {}
  virtual ~TestHtmlDialogUIDelegate() {}

  // HTMLDialogUIDelegate implementation:
  virtual bool IsDialogModal() const OVERRIDE {
    return true;
  }
  virtual string16 GetDialogTitle() const OVERRIDE {
    return UTF8ToUTF16("Test");
  }
  virtual GURL GetDialogContentURL() const OVERRIDE {
    return GURL(chrome::kChromeUIConstrainedHTMLTestURL);
  }
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE {}
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE {
    size->set_width(400);
    size->set_height(400);
  }
  virtual std::string GetDialogArgs() const OVERRIDE {
    return std::string();
  }
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE { }
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog)
      OVERRIDE {
    if (out_close_dialog)
      *out_close_dialog = true;
  }
  virtual bool ShouldShowDialogTitle() const OVERRIDE { return true; }
};

class ConstrainedHtmlDialogBrowserTestObserver : public TabContentsObserver {
 public:
  explicit ConstrainedHtmlDialogBrowserTestObserver(TabContents* contents)
      : TabContentsObserver(contents),
        tab_destroyed_(false) {
  }
  virtual ~ConstrainedHtmlDialogBrowserTestObserver() {}

  bool tab_destroyed() { return tab_destroyed_; }

 private:
  virtual void TabContentsDestroyed(TabContents* tab) {
    tab_destroyed_ = true;
  }

  bool tab_destroyed_;
};

}  // namespace

class ConstrainedHtmlDialogBrowserTest : public InProcessBrowserTest {
 public:
  ConstrainedHtmlDialogBrowserTest() {}

 protected:
  size_t GetConstrainedWindowCount(TabContentsWrapper* wrapper) const {
    return wrapper->constrained_window_tab_helper()->constrained_window_count();
  }
};

// Tests that opening/closing the constrained window won't crash it.
IN_PROC_BROWSER_TEST_F(ConstrainedHtmlDialogBrowserTest, BasicTest) {
  // The delegate deletes itself.
  HtmlDialogUIDelegate* delegate = new TestHtmlDialogUIDelegate();
  TabContentsWrapper* wrapper = browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(wrapper);

  ConstrainedHtmlUIDelegate* html_ui_delegate =
      ConstrainedHtmlUI::CreateConstrainedHtmlDialog(browser()->profile(),
                                                     delegate,
                                                     wrapper);
  ASSERT_TRUE(html_ui_delegate);
  EXPECT_TRUE(html_ui_delegate->window());
  EXPECT_EQ(1U, GetConstrainedWindowCount(wrapper));
}

// Tests that ReleaseTabContentsOnDialogClose() works.
IN_PROC_BROWSER_TEST_F(ConstrainedHtmlDialogBrowserTest,
                       ReleaseTabContentsOnDialogClose) {
  // The delegate deletes itself.
  TestHtmlDialogUIDelegate* delegate = new TestHtmlDialogUIDelegate();
  TabContentsWrapper* wrapper = browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(wrapper);

  ConstrainedHtmlUIDelegate* html_ui_delegate =
      ConstrainedHtmlUI::CreateConstrainedHtmlDialog(browser()->profile(),
                                                     delegate,
                                                     wrapper);
  ASSERT_TRUE(html_ui_delegate);
  scoped_ptr<TabContentsWrapper> new_tab(html_ui_delegate->tab());
  ASSERT_TRUE(new_tab.get());
  ASSERT_EQ(1U, GetConstrainedWindowCount(wrapper));

  ConstrainedHtmlDialogBrowserTestObserver observer(new_tab->tab_contents());
  html_ui_delegate->ReleaseTabContentsOnDialogClose();
  html_ui_delegate->OnDialogCloseFromWebUI();

  ASSERT_FALSE(observer.tab_destroyed());
  EXPECT_EQ(0U, GetConstrainedWindowCount(wrapper));
  new_tab.reset();
  EXPECT_TRUE(observer.tab_destroyed());
}
