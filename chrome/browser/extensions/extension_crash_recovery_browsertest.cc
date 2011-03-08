// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"
#include "chrome/browser/extensions/crashed_extension_infobar.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/result_codes.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"

class ExtensionCrashRecoveryTest : public ExtensionBrowserTest {
 protected:
  ExtensionService* GetExtensionService() {
    return browser()->profile()->GetExtensionService();
  }

  ExtensionProcessManager* GetExtensionProcessManager() {
    return browser()->profile()->GetExtensionProcessManager();
  }

  ConfirmInfoBarDelegate* GetInfoBarDelegate(size_t index) {
    TabContents* current_tab = browser()->GetSelectedTabContents();
    EXPECT_LT(index, current_tab->infobar_count());
    return current_tab->GetInfoBarDelegateAt(index)->AsConfirmInfoBarDelegate();
  }

  void AcceptInfoBar(size_t index) {
    ConfirmInfoBarDelegate* infobar = GetInfoBarDelegate(index);
    ASSERT_TRUE(infobar);
    infobar->Accept();
    WaitForExtensionLoad();
  }

  void CancelInfoBar(size_t index) {
    ConfirmInfoBarDelegate* infobar = GetInfoBarDelegate(index);
    ASSERT_TRUE(infobar);
    infobar->Cancel();
  }

  void CrashExtension(size_t index) {
    ASSERT_LT(index, GetExtensionService()->extensions()->size());
    const Extension* extension =
        GetExtensionService()->extensions()->at(index);
    ASSERT_TRUE(extension);
    std::string extension_id(extension->id());
    ExtensionHost* extension_host =
        GetExtensionProcessManager()->GetBackgroundHostForExtension(extension);
    ASSERT_TRUE(extension_host);

    RenderProcessHost* extension_rph =
        extension_host->render_view_host()->process();
    base::KillProcess(extension_rph->GetHandle(), ResultCodes::KILLED, false);
    ASSERT_TRUE(WaitForExtensionCrash(extension_id));
    ASSERT_FALSE(
        GetExtensionProcessManager()->GetBackgroundHostForExtension(extension));
  }

  void CheckExtensionConsistency(size_t index) {
    ASSERT_LT(index, GetExtensionService()->extensions()->size());
    const Extension* extension =
        GetExtensionService()->extensions()->at(index);
    ASSERT_TRUE(extension);
    ExtensionHost* extension_host =
        GetExtensionProcessManager()->GetBackgroundHostForExtension(extension);
    ASSERT_TRUE(extension_host);
    ASSERT_TRUE(GetExtensionProcessManager()->HasExtensionHost(extension_host));
    ASSERT_TRUE(extension_host->IsRenderViewLive());
    ASSERT_EQ(extension_host->render_view_host()->process(),
        GetExtensionProcessManager()->GetExtensionProcess(extension->id()));
  }

  void LoadTestExtension() {
    ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
    const size_t size_before = GetExtensionService()->extensions()->size();
    ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII("common").AppendASCII("background_page")));
    const Extension* extension = GetExtensionService()->extensions()->back();
    ASSERT_TRUE(extension);
    first_extension_id_ = extension->id();
    CheckExtensionConsistency(size_before);
  }

  void LoadSecondExtension() {
    int offset = GetExtensionService()->extensions()->size();
    ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII("install").AppendASCII("install")));
    const Extension* extension =
        GetExtensionService()->extensions()->at(offset);
    ASSERT_TRUE(extension);
    second_extension_id_ = extension->id();
    CheckExtensionConsistency(offset);
  }

  std::string first_extension_id_;
  std::string second_extension_id_;
};

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, Basic) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());
  AcceptInfoBar(0);

  SCOPED_TRACE("after clicking the infobar");
  CheckExtensionConsistency(size_before);
  ASSERT_EQ(crash_size_before,
            GetExtensionService()->terminated_extensions()->size());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, CloseAndReload) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());

  CancelInfoBar(0);
  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(size_before);
  ASSERT_EQ(crash_size_before,
            GetExtensionService()->terminated_extensions()->size());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, ReloadIndependently) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(size_before);

  TabContents* current_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(current_tab);

  // The infobar should automatically hide after the extension is successfully
  // reloaded.
  ASSERT_EQ(0U, current_tab->infobar_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       ReloadIndependentlyChangeTabs) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  TabContents* original_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(original_tab);
  ASSERT_EQ(1U, original_tab->infobar_count());

  // Open a new tab so the info bar will not be in the current tab.
  browser()->NewTab();
  TabContents* new_current_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(new_current_tab);
  ASSERT_NE(new_current_tab, original_tab);
  ASSERT_EQ(0U, new_current_tab->infobar_count());

  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(size_before);

  // The infobar should automatically hide after the extension is successfully
  // reloaded.
  ASSERT_EQ(0U, original_tab->infobar_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       ReloadIndependentlyNavigatePage) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  TabContents* current_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(current_tab);
  ASSERT_EQ(1U, current_tab->infobar_count());

  // Navigate to another page.
  ui_test_utils::NavigateToURL(browser(),
      ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                FilePath(FILE_PATH_LITERAL("title1.html"))));
  ASSERT_EQ(1U, current_tab->infobar_count());

  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(size_before);

  // The infobar should automatically hide after the extension is successfully
  // reloaded.
  ASSERT_EQ(0U, current_tab->infobar_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       ReloadIndependentlyTwoInfoBars) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();

  // Open a new window so that there will be an info bar in each.
  Browser* browser2 = CreateBrowser(browser()->profile());

  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  TabContents* current_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(current_tab);
  ASSERT_EQ(1U, current_tab->infobar_count());

  TabContents* current_tab2 = browser2->GetSelectedTabContents();
  ASSERT_TRUE(current_tab2);
  ASSERT_EQ(1U, current_tab2->infobar_count());

  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(size_before);

  // Both infobars should automatically hide after the extension is successfully
  // reloaded.
  ASSERT_EQ(0U, current_tab->infobar_count());
  ASSERT_EQ(0U, current_tab2->infobar_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       ReloadIndependentlyTwoInfoBarsSameBrowser) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();

  // Open a new window so that there will be an info bar in each.
  Browser* browser2 = CreateBrowser(browser()->profile());

  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  TabContents* current_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(current_tab);
  ASSERT_EQ(1U, current_tab->infobar_count());

  TabContents* current_tab2 = browser2->GetSelectedTabContents();
  ASSERT_TRUE(current_tab2);
  ASSERT_EQ(1U, current_tab2->infobar_count());

  // Move second window into first browser so there will be multiple tabs
  // with the info bar for the same extension in one browser.
  TabContentsWrapper* contents =
      browser2->tabstrip_model()->DetachTabContentsAt(0);
  browser()->tabstrip_model()->AppendTabContents(contents, true);
  current_tab2 = browser()->GetSelectedTabContents();
  ASSERT_EQ(1U, current_tab2->infobar_count());
  ASSERT_NE(current_tab2, current_tab);

  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(size_before);

  // Both infobars should automatically hide after the extension is successfully
  // reloaded.
  ASSERT_EQ(0U, current_tab2->infobar_count());
  browser()->SelectPreviousTab();
  ASSERT_EQ(current_tab, browser()->GetSelectedTabContents());
  ASSERT_EQ(0U, current_tab->infobar_count());
}

// Make sure that when we don't do anything about the crashed extension
// and close the browser, it doesn't crash. The browser is closed implicitly
// at the end of each browser test.
IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, ShutdownWhileCrashed) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, TwoExtensionsCrashFirst) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  AcceptInfoBar(0);

  SCOPED_TRACE("after clicking the infobar");
  CheckExtensionConsistency(size_before);
  CheckExtensionConsistency(size_before + 1);
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, TwoExtensionsCrashSecond) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(size_before + 1);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  AcceptInfoBar(0);

  SCOPED_TRACE("after clicking the infobar");
  CheckExtensionConsistency(size_before);
  CheckExtensionConsistency(size_before + 1);
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       TwoExtensionsCrashBothAtOnce) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 2,
            GetExtensionService()->terminated_extensions()->size());

  {
    SCOPED_TRACE("first infobar");
    AcceptInfoBar(0);
    CheckExtensionConsistency(size_before);
  }

  {
    SCOPED_TRACE("second infobar");
    AcceptInfoBar(0);
    CheckExtensionConsistency(size_before);
    CheckExtensionConsistency(size_before + 1);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, TwoExtensionsOneByOne) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  {
    SCOPED_TRACE("first infobar");
    AcceptInfoBar(0);
    CheckExtensionConsistency(size_before);
  }

  {
    SCOPED_TRACE("second infobar");
    AcceptInfoBar(0);
    CheckExtensionConsistency(size_before);
    CheckExtensionConsistency(size_before + 1);
  }
}

// Make sure that when we don't do anything about the crashed extensions
// and close the browser, it doesn't crash. The browser is closed implicitly
// at the end of each browser test.
IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       TwoExtensionsShutdownWhileCrashed) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
}

// http://crbug.com/75134 Crashes on Win/ChromeOS only.
#if defined(OS_WIN) || defined(OS_CHROMEOS)
#define MAYBE_TwoExtensionsIgnoreFirst DISABLED_TwoExtensionsIgnoreFirst
#else
#define MAYBE_TwoExtensionsIgnoreFirst TwoExtensionsIgnoreFirst
#endif
IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       MAYBE_TwoExtensionsIgnoreFirst) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  CancelInfoBar(0);
  AcceptInfoBar(1);

  SCOPED_TRACE("infobars done");
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  CheckExtensionConsistency(size_before);
}

// Marked as flaky due to http://crbug.com/75134
IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       FLAKY_TwoExtensionsReloadIndependently) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  {
    SCOPED_TRACE("first: reload");
    TabContents* current_tab = browser()->GetSelectedTabContents();
    ASSERT_TRUE(current_tab);
    // At the beginning we should have one infobar displayed for each extension.
    ASSERT_EQ(2U, current_tab->infobar_count());
    ReloadExtension(first_extension_id_);
    // One of the infobars should hide after the extension is reloaded.
    ASSERT_EQ(1U, current_tab->infobar_count());
    CheckExtensionConsistency(size_before);
  }

  {
    SCOPED_TRACE("second: infobar");
    AcceptInfoBar(0);
    CheckExtensionConsistency(size_before);
    CheckExtensionConsistency(size_before + 1);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, CrashAndUninstall) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());

  UninstallExtension(first_extension_id_);

  SCOPED_TRACE("after uninstalling");
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before,
            GetExtensionService()->terminated_extensions()->size());
  ASSERT_EQ(0U, browser()->GetSelectedTabContents()->infobar_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, CrashAndUnloadAll) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());

  GetExtensionService()->UnloadAllExtensions();
  ASSERT_EQ(crash_size_before,
            GetExtensionService()->terminated_extensions()->size());
}
