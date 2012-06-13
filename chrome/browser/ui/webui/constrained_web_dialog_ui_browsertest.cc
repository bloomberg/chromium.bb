// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/test_web_dialog_delegate.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/web_dialogs/constrained_web_dialog_ui.h"

using content::WebContents;
using ui::ConstrainedWebDialogDelegate;
using ui::WebDialogDelegate;

namespace {

class ConstrainedWebDialogBrowserTestObserver
    : public content::WebContentsObserver {
 public:
  explicit ConstrainedWebDialogBrowserTestObserver(WebContents* contents)
      : content::WebContentsObserver(contents),
        tab_destroyed_(false) {
  }
  virtual ~ConstrainedWebDialogBrowserTestObserver() {}

  bool tab_destroyed() { return tab_destroyed_; }

 private:
  virtual void WebContentsDestroyed(WebContents* tab) OVERRIDE {
    tab_destroyed_ = true;
  }

  bool tab_destroyed_;
};

}  // namespace

class ConstrainedWebDialogBrowserTest : public InProcessBrowserTest {
 public:
  ConstrainedWebDialogBrowserTest() {}

 protected:
  size_t GetConstrainedWindowCount(TabContents* tab_contents) const {
    return tab_contents->
        constrained_window_tab_helper()->constrained_window_count();
  }
};

// Tests that opening/closing the constrained window won't crash it.
IN_PROC_BROWSER_TEST_F(ConstrainedWebDialogBrowserTest, BasicTest) {
  // The delegate deletes itself.
  WebDialogDelegate* delegate = new test::TestWebDialogDelegate(
      GURL(chrome::kChromeUIConstrainedHTMLTestURL));
  TabContents* tab_contents = browser()->GetActiveTabContents();
  ASSERT_TRUE(tab_contents);

  ConstrainedWebDialogDelegate* dialog_delegate =
      ui::CreateConstrainedWebDialog(browser()->profile(),
                                     delegate,
                                     NULL,
                                     tab_contents);
  ASSERT_TRUE(dialog_delegate);
  EXPECT_TRUE(dialog_delegate->window());
  EXPECT_EQ(1U, GetConstrainedWindowCount(tab_contents));
}

// Tests that ReleaseTabContentsOnDialogClose() works.
IN_PROC_BROWSER_TEST_F(ConstrainedWebDialogBrowserTest,
                       ReleaseTabContentsOnDialogClose) {
  // The delegate deletes itself.
  WebDialogDelegate* delegate = new test::TestWebDialogDelegate(
      GURL(chrome::kChromeUIConstrainedHTMLTestURL));
  TabContents* tab_contents = browser()->GetActiveTabContents();
  ASSERT_TRUE(tab_contents);

  ConstrainedWebDialogDelegate* dialog_delegate =
      ui::CreateConstrainedWebDialog(browser()->profile(),
                                     delegate,
                                     NULL,
                                     tab_contents);
  ASSERT_TRUE(dialog_delegate);
  scoped_ptr<TabContents> new_tab(dialog_delegate->tab());
  ASSERT_TRUE(new_tab.get());
  ASSERT_EQ(1U, GetConstrainedWindowCount(tab_contents));

  ConstrainedWebDialogBrowserTestObserver observer(new_tab->web_contents());
  dialog_delegate->ReleaseTabContentsOnDialogClose();
  dialog_delegate->OnDialogCloseFromWebUI();

  ASSERT_FALSE(observer.tab_destroyed());
  EXPECT_EQ(0U, GetConstrainedWindowCount(tab_contents));
  new_tab.reset();
  EXPECT_TRUE(observer.tab_destroyed());
}
