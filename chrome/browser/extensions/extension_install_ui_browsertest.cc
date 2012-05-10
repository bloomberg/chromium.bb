// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

class ExtensionInstallUIBrowserTest : public ExtensionBrowserTest {
 public:
  // Checks that a theme info bar is currently visible and issues an undo to
  // revert to the previous theme.
  void VerifyThemeInfoBarAndUndoInstall() {
    TabContentsWrapper* tab = browser()->GetSelectedTabContentsWrapper();
    ASSERT_TRUE(tab);
    InfoBarTabHelper* infobar_helper = tab->infobar_tab_helper();
    ASSERT_EQ(1U, infobar_helper->infobar_count());
    ConfirmInfoBarDelegate* delegate = infobar_helper->
        GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
    ASSERT_TRUE(delegate);
    delegate->Cancel();
    ASSERT_EQ(0U, infobar_helper->infobar_count());
  }

  const Extension* GetTheme() const {
    return ThemeServiceFactory::GetThemeForProfile(browser()->profile());
  }
};

#if defined(OS_LINUX)
// Fails consistently on bot chromium.chromiumos \ Linux.
// See: http://crbug.com/120647.
#define MAYBE_TestThemeInstallUndoResetsToDefault DISABLED_TestThemeInstallUndoResetsToDefault
#else
#define MAYBE_TestThemeInstallUndoResetsToDefault TestThemeInstallUndoResetsToDefault
#endif

IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       MAYBE_TestThemeInstallUndoResetsToDefault) {
  // Install theme once and undo to verify we go back to default theme.
  FilePath theme_crx = PackExtension(test_data_dir_.AppendASCII("theme"));
  ASSERT_TRUE(InstallExtensionWithUI(theme_crx, 1));
  const Extension* theme = GetTheme();
  ASSERT_TRUE(theme);
  std::string theme_id = theme->id();
  VerifyThemeInfoBarAndUndoInstall();
  ASSERT_EQ(NULL, GetTheme());

  // Set the same theme twice and undo to verify we go back to default theme.
  // We set the |expected_change| to zero in these 'InstallExtensionWithUI'
  // calls since the theme has already been installed above and this is an
  // overinstall to set the active theme.
  ASSERT_TRUE(InstallExtensionWithUI(theme_crx, 0));
  theme = GetTheme();
  ASSERT_TRUE(theme);
  ASSERT_EQ(theme_id, theme->id());
  ASSERT_TRUE(InstallExtensionWithUI(theme_crx, 0));
  theme = GetTheme();
  ASSERT_TRUE(theme);
  ASSERT_EQ(theme_id, theme->id());
  VerifyThemeInfoBarAndUndoInstall();
  ASSERT_EQ(NULL, GetTheme());
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       TestThemeInstallUndoResetsToPreviousTheme) {
  // Install first theme.
  FilePath theme_path = test_data_dir_.AppendASCII("theme");
  ASSERT_TRUE(InstallExtensionWithUI(theme_path, 1));
  const Extension* theme = GetTheme();
  ASSERT_TRUE(theme);
  std::string theme_id = theme->id();

  // Then install second theme.
  FilePath theme_path2 = test_data_dir_.AppendASCII("theme2");
  ASSERT_TRUE(InstallExtensionWithUI(theme_path2, 1));
  const Extension* theme2 = GetTheme();
  ASSERT_TRUE(theme2);
  EXPECT_FALSE(theme_id == theme2->id());

  // Undo second theme will revert to first theme.
  VerifyThemeInfoBarAndUndoInstall();
  EXPECT_EQ(theme, GetTheme());
}

// Disabled due to a use-after-free, see http://crbug.com/127535.
IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       DISABLED_AppInstallConfirmation) {
  int num_tabs = browser()->tab_count();

  FilePath app_dir = test_data_dir_.AppendASCII("app");
  ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(app_dir, 1,
                                                browser()->profile()));

  if (NewTabUI::ShouldShowApps()) {
    EXPECT_EQ(num_tabs + 1, browser()->tab_count());
    WebContents* web_contents = browser()->GetSelectedWebContents();
    ASSERT_TRUE(web_contents);
    EXPECT_TRUE(StartsWithASCII(web_contents->GetURL().spec(),
                                "chrome://newtab/", false));
  } else {
    // TODO(xiyuan): Figure out how to test extension installed bubble?
  }
}

// Disabled due to a use-after-free, see http://crbug.com/127535.
IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       DISABLED_AppInstallConfirmation_Incognito) {
  Profile* incognito_profile = browser()->profile()->GetOffTheRecordProfile();
  Browser* incognito_browser = Browser::GetOrCreateTabbedBrowser(
      incognito_profile);

  int num_incognito_tabs = incognito_browser->tab_count();
  int num_normal_tabs = browser()->tab_count();

  FilePath app_dir = test_data_dir_.AppendASCII("app");
  ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(app_dir, 1,
                                                incognito_profile));

  EXPECT_EQ(num_incognito_tabs, incognito_browser->tab_count());
  if (NewTabUI::ShouldShowApps()) {
    EXPECT_EQ(num_normal_tabs + 1, browser()->tab_count());
    WebContents* web_contents = browser()->GetSelectedWebContents();
    ASSERT_TRUE(web_contents);
    EXPECT_TRUE(StartsWithASCII(web_contents->GetURL().spec(),
                                "chrome://newtab/", false));
  } else {
    // TODO(xiyuan): Figure out how to test extension installed bubble?
  }
}
