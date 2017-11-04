// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_impl.h"

class NewTabPromoDialogTest : public DialogBrowserTest {
 public:
  NewTabPromoDialogTest() = default;

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    // The promo only exists in the TabStripImpl.
    TabStripImpl* tab_strip_impl =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->tabstrip()
            ->AsTabStripImpl();
    if (tab_strip_impl)
      tab_strip_impl->new_tab_button()->ShowPromo();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NewTabPromoDialogTest);
};

// Test that calls ShowDialog("default"). Interactive when run via
// ../browser_tests --gtest_filter=BrowserDialogTest.Invoke
// --interactive --dialog=NewTabPromoDialogTest.InvokeDialog_NewTabPromo
IN_PROC_BROWSER_TEST_F(NewTabPromoDialogTest, InvokeDialog_NewTabPromo) {
  RunDialog();
}
