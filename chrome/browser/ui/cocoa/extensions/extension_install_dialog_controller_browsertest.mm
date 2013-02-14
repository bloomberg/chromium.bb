// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_install_dialog_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_prompt_test_utils.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/in_process_browser_test.h"

using extensions::Extension;

class ExtensionInstallDialogControllerTest : public InProcessBrowserTest {
public:
  ExtensionInstallDialogControllerTest() {
    extension_ = chrome::LoadInstallPromptExtension();
  }

 protected:
  scoped_refptr<Extension> extension_;
};

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogControllerTest, BasicTest) {
  content::WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(0);
  ExtensionInstallPrompt::ShowParams show_params(tab);

  chrome::MockExtensionInstallPromptDelegate delegate;
  ExtensionInstallPrompt::Prompt prompt =
      chrome::BuildExtensionInstallPrompt(extension_);

  ExtensionInstallDialogController* controller =
      new ExtensionInstallDialogController(show_params,
                                           &delegate,
                                           prompt);

  scoped_nsobject<NSWindow> window(
      [[[controller->view_controller() view] window] retain]);
  EXPECT_TRUE([window isVisible]);

  // Press cancel to close the window
  [[controller->view_controller() cancelButton] performClick:nil];

  // Wait for the window to finish closing.
  EXPECT_FALSE([window isVisible]);
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogControllerTest, Permissions) {
  content::WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(0);
  ExtensionInstallPrompt::ShowParams show_params(tab);

  chrome::MockExtensionInstallPromptDelegate delegate;
  ExtensionInstallPrompt::Prompt prompt =
      chrome::BuildExtensionPostInstallPermissionsPrompt(extension_);

  ExtensionInstallDialogController* controller =
      new ExtensionInstallDialogController(show_params,
                                           &delegate,
                                           prompt);

  scoped_nsobject<NSWindow> window(
      [[[controller->view_controller() view] window] retain]);
  EXPECT_TRUE([window isVisible]);

  // Press cancel to close the window
  [[controller->view_controller() cancelButton] performClick:nil];

  // Wait for the window to finish closing.
  EXPECT_FALSE([window isVisible]);
}
