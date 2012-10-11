// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_install_dialog_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_controller.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_view_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

ExtensionInstallDialogController::ExtensionInstallDialogController(
    content::WebContents* webContents,
    content::PageNavigator* navigator,
    ExtensionInstallPrompt::Delegate* delegate,
    const ExtensionInstallPrompt::Prompt& prompt) : delegate_(delegate) {
  view_controller_.reset([[ExtensionInstallViewController alloc]
      initWithNavigator:navigator
               delegate:this
                 prompt:prompt]);
  window_controller_.reset([[ConstrainedWindowController alloc]
      initWithParentWebContents:webContents
                   embeddedView:[view_controller_ view]]);
}

ExtensionInstallDialogController::~ExtensionInstallDialogController() {
}

void ExtensionInstallDialogController::InstallUIProceed() {
  delegate_->InstallUIProceed();
  [window_controller_ close];
  delete this;
}

void ExtensionInstallDialogController::InstallUIAbort(bool user_initiated) {
  delegate_->InstallUIAbort(user_initiated);
  [window_controller_ close];
  delete this;
}

void ShowExtensionInstallDialogImpl(
    gfx::NativeWindow parent,
    content::PageNavigator* navigator,
    ExtensionInstallPrompt::Delegate* delegate,
    const ExtensionInstallPrompt::Prompt& prompt) {
  // TODO(sail) Update ShowExtensionInstallDialogImpl to take a web contents.
  Browser* browser = browser::FindBrowserWithWindow(parent);
  if (!browser)
    return;
  TabContents* tab = browser->tab_strip_model()->GetActiveTabContents();
  if (!tab)
    return;

  // This object will delete itself when the dialog closes.
  new ExtensionInstallDialogController(tab->web_contents(),
                                       navigator,
                                       delegate,
                                       prompt);
}
