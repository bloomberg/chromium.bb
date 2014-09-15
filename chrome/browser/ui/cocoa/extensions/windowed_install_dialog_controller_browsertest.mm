// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/windowed_install_dialog_controller.h"

#include "base/run_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_prompt_test_utils.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_view_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"

namespace {

// Similar to ShowExtensionInstallDialogImpl except this allows the created
// dialog controller to be captured and manipulated for tests.
void TestingShowAppListInstallDialogController(
    WindowedInstallDialogController** controller,
    const ExtensionInstallPrompt::ShowParams& show_params,
    ExtensionInstallPrompt::Delegate* delegate,
    scoped_refptr<ExtensionInstallPrompt::Prompt> prompt) {
  *controller =
      new WindowedInstallDialogController(show_params, delegate, prompt);
}

typedef InProcessBrowserTest WindowedInstallDialogControllerBrowserTest;

}  // namespace

// Test for showing an extension install prompt with no parent WebContents.
IN_PROC_BROWSER_TEST_F(WindowedInstallDialogControllerBrowserTest,
                       ShowInstallDialog) {
  // Construct a prompt with a NULL parent window, the way ExtensionEnableFlow
  // will for the Mac app list. For testing, sets a NULL PageNavigator as well.
  scoped_ptr<ExtensionInstallPrompt> prompt(
      new ExtensionInstallPrompt(browser()->profile(), NULL, NULL));

  WindowedInstallDialogController* controller = NULL;
  chrome::MockExtensionInstallPromptDelegate delegate;
  scoped_refptr<extensions::Extension> extension =
      chrome::LoadInstallPromptExtension("permissions", "many-apis.json");
  prompt->ConfirmInstall(
      &delegate,
      extension.get(),
      base::Bind(&TestingShowAppListInstallDialogController, &controller));

  // The prompt needs to load the image, which happens on the blocking pool.
  content::RunAllBlockingPoolTasksUntilIdle();
  ASSERT_TRUE(controller);

  base::scoped_nsobject<NSWindow> window(
      [[[controller->GetViewController() view] window] retain]);
  EXPECT_TRUE([window isVisible]);
  EXPECT_TRUE([window delegate]);
  EXPECT_EQ(0, delegate.abort_count());

  // Press cancel to close the window.
  [[controller->GetViewController() cancelButton] performClick:nil];
  EXPECT_FALSE([window delegate]);
  EXPECT_EQ(1, delegate.abort_count());

  // Ensure the window is closed.
  EXPECT_FALSE([window isVisible]);
}
