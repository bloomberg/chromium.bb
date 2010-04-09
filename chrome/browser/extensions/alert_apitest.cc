// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_modal_dialog.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/profile.h"
#include "chrome/test/ui_test_utils.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, AlertBasic) {
  ASSERT_TRUE(RunExtensionTest("alert")) << message_;

  Extension* extension = GetSingleLoadedExtension();
  ExtensionHost* host = browser()->profile()->GetExtensionProcessManager()->
      GetBackgroundHostForExtension(extension);
  ASSERT_TRUE(host);

  host->render_view_host()->ExecuteJavascriptInWebFrame(L"",
      L"alert('This should not crash.');");

  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  ASSERT_TRUE(alert);
  ASSERT_TRUE(alert->IsValid());

  alert->CloseModalDialog();
}

// Test that we handle the case of an extension being unloaded while an alert is
// up gracefully.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, AlertOrphanedByUnload) {
  ASSERT_TRUE(RunExtensionTest("alert")) << message_;

  Extension* extension = GetSingleLoadedExtension();
  ExtensionHost* host = browser()->profile()->GetExtensionProcessManager()->
      GetBackgroundHostForExtension(extension);
  ASSERT_TRUE(host);

  host->render_view_host()->ExecuteJavascriptInWebFrame(L"",
      L"alert('This should not crash.');");

  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  ASSERT_TRUE(alert);

  browser()->profile()->GetExtensionsService()->UnloadExtension(
      extension->id());

  ASSERT_FALSE(alert->IsValid());
}

// Test that we handle the case of an extension crashing while an alert is up
// gracefully.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, AlertOrphanedByCrash) {
  ASSERT_TRUE(RunExtensionTest("alert")) << message_;

  Extension* extension = GetSingleLoadedExtension();
  ExtensionHost* host = browser()->profile()->GetExtensionProcessManager()->
      GetBackgroundHostForExtension(extension);
  ASSERT_TRUE(host);

  host->render_view_host()->ExecuteJavascriptInWebFrame(L"",
      L"alert('This should not crash.');");

  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  ASSERT_TRUE(alert);

  base::KillProcess(
    browser()->profile()->GetExtensionProcessManager()->GetExtensionProcess(
        extension->id())->GetHandle(),
    base::PROCESS_END_KILLED_BY_USER,
    true);  // Wait for process to exit.
}
