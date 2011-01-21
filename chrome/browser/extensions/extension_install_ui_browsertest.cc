// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/ui_test_utils.h"

namespace {

// Theme ID used for testing.
const char* const theme_crx = "iamefpfkojoapidjnbafmgkgncegbkad";

}  // namespace

class ExtensionInstallUIBrowserTest : public ExtensionBrowserTest {
 public:
  // Checks that a theme info bar is currently visible and issues an undo to
  // revert to the previous theme.
  void VerifyThemeInfoBarAndUndoInstall() {
    TabContents* tab_contents = browser()->GetSelectedTabContents();
    ASSERT_TRUE(tab_contents);
    ASSERT_EQ(1, tab_contents->infobar_delegate_count());
    ConfirmInfoBarDelegate* delegate =
        tab_contents->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
    ASSERT_TRUE(delegate);
    delegate->Cancel();
    ASSERT_EQ(0, tab_contents->infobar_delegate_count());
  }
};

// Crashy, http://crbug.com/44548.
IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       DISABLED_TestThemeInstallUndoResetsToDefault) {
  // Install theme once and undo to verify we go back to default theme.
  FilePath theme_path = test_data_dir_.AppendASCII("theme.crx");
  ASSERT_TRUE(InstallExtensionWithUI(theme_path, 1));
  const Extension* theme = browser()->profile()->GetTheme();
  ASSERT_TRUE(theme);
  ASSERT_EQ(theme_crx, theme->id());
  VerifyThemeInfoBarAndUndoInstall();
  ASSERT_EQ(NULL, browser()->profile()->GetTheme());

  // Set the same theme twice and undo to verify we go back to default theme.
  // We set the |expected_change| to zero in these 'InstallExtensionWithUI'
  // calls since the theme has already been installed above and this is an
  // overinstall to set the active theme.
  ASSERT_TRUE(InstallExtensionWithUI(theme_path, 0));
  theme = browser()->profile()->GetTheme();
  ASSERT_TRUE(theme);
  ASSERT_EQ(theme_crx, theme->id());
  ASSERT_TRUE(InstallExtensionWithUI(theme_path, 0));
  theme = browser()->profile()->GetTheme();
  ASSERT_TRUE(theme);
  ASSERT_EQ(theme_crx, theme->id());
  VerifyThemeInfoBarAndUndoInstall();
  ASSERT_EQ(NULL, browser()->profile()->GetTheme());
}
