// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/web_dialogs/test/test_web_dialog_delegate.h"

using content::WebContents;
using ui::WebDialogDelegate;
using web_modal::WebContentsModalDialogManager;

namespace {

class ConstrainedWebDialogBrowserTestObserver
    : public content::WebContentsObserver {
 public:
  explicit ConstrainedWebDialogBrowserTestObserver(WebContents* contents)
      : content::WebContentsObserver(contents),
        contents_destroyed_(false) {
  }
  virtual ~ConstrainedWebDialogBrowserTestObserver() {}

  bool contents_destroyed() { return contents_destroyed_; }

 private:
  virtual void WebContentsDestroyed() OVERRIDE {
    contents_destroyed_ = true;
  }

  bool contents_destroyed_;
};

}  // namespace

class ConstrainedWebDialogBrowserTest : public InProcessBrowserTest {
 public:
  ConstrainedWebDialogBrowserTest() {}

 protected:
  bool IsShowingWebContentsModalDialog(WebContents* web_contents) const {
    WebContentsModalDialogManager* web_contents_modal_dialog_manager =
        WebContentsModalDialogManager::FromWebContents(web_contents);
    return web_contents_modal_dialog_manager->IsDialogActive();
  }
};

// Tests that opening/closing the constrained window won't crash it.
IN_PROC_BROWSER_TEST_F(ConstrainedWebDialogBrowserTest, BasicTest) {
  // The delegate deletes itself.
  WebDialogDelegate* delegate = new ui::test::TestWebDialogDelegate(
      GURL(chrome::kChromeUIConstrainedHTMLTestURL));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  ConstrainedWebDialogDelegate* dialog_delegate =
      CreateConstrainedWebDialog(browser()->profile(), delegate, web_contents);
  ASSERT_TRUE(dialog_delegate);
  EXPECT_TRUE(dialog_delegate->GetNativeDialog());
  EXPECT_TRUE(IsShowingWebContentsModalDialog(web_contents));
}

// Tests that ReleaseWebContentsOnDialogClose() works.
IN_PROC_BROWSER_TEST_F(ConstrainedWebDialogBrowserTest,
                       ReleaseWebContentsOnDialogClose) {
  // The delegate deletes itself.
  WebDialogDelegate* delegate = new ui::test::TestWebDialogDelegate(
      GURL(chrome::kChromeUIConstrainedHTMLTestURL));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  ConstrainedWebDialogDelegate* dialog_delegate =
      CreateConstrainedWebDialog(browser()->profile(), delegate, web_contents);
  ASSERT_TRUE(dialog_delegate);
  scoped_ptr<WebContents> new_tab(dialog_delegate->GetWebContents());
  ASSERT_TRUE(new_tab.get());
  ASSERT_TRUE(IsShowingWebContentsModalDialog(web_contents));

  ConstrainedWebDialogBrowserTestObserver observer(new_tab.get());
  dialog_delegate->ReleaseWebContentsOnDialogClose();
  dialog_delegate->OnDialogCloseFromWebUI();

  ASSERT_FALSE(observer.contents_destroyed());
  EXPECT_FALSE(IsShowingWebContentsModalDialog(web_contents));
  new_tab.reset();
  EXPECT_TRUE(observer.contents_destroyed());
}
