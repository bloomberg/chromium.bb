// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/crashed_extension_infobar.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/test/ui_test_utils.h"

class ExtensionCrashRecoveryTest : public ExtensionBrowserTest {
 protected:
  ExtensionsService* GetExtensionsService() {
    return browser()->profile()->GetExtensionsService();
  }

  ExtensionProcessManager* GetExtensionProcessManager() {
    return browser()->profile()->GetExtensionProcessManager();
  }

  CrashedExtensionInfoBarDelegate* GetCrashedExtensionInfoBarDelegate() {
    TabContents* current_tab = browser()->GetSelectedTabContents();
    EXPECT_EQ(1, current_tab->infobar_delegate_count());
    InfoBarDelegate* delegate = current_tab->GetInfoBarDelegateAt(0);
    return delegate->AsCrashedExtensionInfoBarDelegate();
  }

  void AcceptCrashedExtensionInfobar() {
    CrashedExtensionInfoBarDelegate* infobar =
        GetCrashedExtensionInfoBarDelegate();
    ASSERT_TRUE(infobar);
    infobar->Accept();
    if (GetExtensionsService()->extensions()->empty())
      WaitForExtensionLoad();
  }

  void CancelCrashedExtensionInfobar() {
    CrashedExtensionInfoBarDelegate* infobar =
        GetCrashedExtensionInfoBarDelegate();
    ASSERT_TRUE(infobar);
    infobar->Cancel();
  }

  void CrashExtension() {
    Extension* extension = GetExtensionsService()->extensions()->at(0);
    ASSERT_TRUE(extension);
    ExtensionHost* extension_host =
        GetExtensionProcessManager()->GetBackgroundHostForExtension(extension);
    ASSERT_TRUE(extension_host);

    RenderProcessHost* extension_rph =
        extension_host->render_view_host()->process();
    base::KillProcess(extension_rph->GetHandle(),
                      base::PROCESS_END_KILLED_BY_USER, false);
    ASSERT_TRUE(WaitForExtensionCrash(extension_id_));
    ASSERT_FALSE(
        GetExtensionProcessManager()->GetBackgroundHostForExtension(extension));
    ASSERT_TRUE(GetExtensionsService()->extensions()->empty());
  }

  void CheckExtensionConsistency() {
    ASSERT_EQ(1U, GetExtensionsService()->extensions()->size());
    Extension* extension = GetExtensionsService()->extensions()->at(0);
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
    ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));
    Extension* extension = GetExtensionsService()->extensions()->at(0);
    ASSERT_TRUE(extension);
    extension_id_ = extension->id();
    CheckExtensionConsistency();
  }

  std::string extension_id_;
};

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, Basic) {
  LoadTestExtension();
  CrashExtension();
  AcceptCrashedExtensionInfobar();

  SCOPED_TRACE("after clicking the infobar");
  CheckExtensionConsistency();
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, CloseAndReload) {
  LoadTestExtension();
  CrashExtension();
  CancelCrashedExtensionInfobar();
  ReloadExtension(extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency();
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, ReloadIndependently) {
  LoadTestExtension();
  CrashExtension();

  ReloadExtension(extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency();

  TabContents* current_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(current_tab);

  // The infobar should automatically hide after the extension is successfully
  // reloaded.
  ASSERT_EQ(0, current_tab->infobar_delegate_count());
}
