// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

class IncognitoWindowPromoDialogTest : public DialogBrowserTest {
 public:
  IncognitoWindowPromoDialogTest() = default;

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    browser_view->toolbar()->app_menu_button()->ShowPromo();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IncognitoWindowPromoDialogTest);
};

// Test that calls ShowDialog("default"). Interactive when run via
// ../browser_tests --gtest_filter=BrowserDialogTest.Invoke
// --interactive
// --dialog=IncognitoWindowPromoDialogTest.InvokeDialog_IncognitoWindowPromo
IN_PROC_BROWSER_TEST_F(IncognitoWindowPromoDialogTest,
                       InvokeDialog_IncognitoWindowPromo) {
  RunDialog();
}
