// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/outdated_upgrade_bubble_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

class OutdatedUpgradeBubbleTest : public DialogBrowserTest {
 public:
  OutdatedUpgradeBubbleTest() = default;

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    ToolbarView* toolbar_view =
        BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
    if (name == "Outdated")
      toolbar_view->OnOutdatedInstall();
    else if (name == "NoAutoUpdate")
      toolbar_view->OnOutdatedInstallNoAutoUpdate();
    else if (name == "Critical")
      toolbar_view->OnCriticalUpgradeInstalled();
    else
      ADD_FAILURE();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OutdatedUpgradeBubbleTest);
};

IN_PROC_BROWSER_TEST_F(OutdatedUpgradeBubbleTest, InvokeDialog_Outdated) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(OutdatedUpgradeBubbleTest, InvokeDialog_NoAutoUpdate) {
  RunDialog();
}

// The critical upgrade dialog is intentionally only shown on Windows.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(OutdatedUpgradeBubbleTest, InvokeDialog_Critical) {
  RunDialog();
}
#endif
