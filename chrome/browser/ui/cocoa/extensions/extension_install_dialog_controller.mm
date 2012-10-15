// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_install_dialog_controller.h"

#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_view_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace {

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

}  // namespace

ExtensionInstallDialogController::ExtensionInstallDialogController(
    content::WebContents* web_contents,
    content::PageNavigator* navigator,
    ExtensionInstallPrompt::Delegate* delegate,
    const ExtensionInstallPrompt::Prompt& prompt) : delegate_(delegate) {
  view_controller_.reset([[ExtensionInstallViewController alloc]
      initWithNavigator:navigator
               delegate:this
                 prompt:prompt]);

  scoped_nsobject<NSWindow> window([[ConstrainedWindowCustomWindow alloc]
      initWithContentRect:[[view_controller_ view] bounds]]);
  [[window contentView] addSubview:[view_controller_ view]];

  constrained_window_.reset(new ConstrainedWindowMac2(
      this, web_contents, window));
}

ExtensionInstallDialogController::~ExtensionInstallDialogController() {
}

void ExtensionInstallDialogController::InstallUIProceed() {
  delegate_->InstallUIProceed();
  delegate_ = NULL;
  constrained_window_->CloseConstrainedWindow();
}

void ExtensionInstallDialogController::InstallUIAbort(bool user_initiated) {
  delegate_->InstallUIAbort(user_initiated);
  delegate_ = NULL;
  constrained_window_->CloseConstrainedWindow();
}

void ExtensionInstallDialogController::OnConstrainedWindowClosed(
    ConstrainedWindowMac2* window) {
  if (delegate_)
    delegate_->InstallUIAbort(false);
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

// static
ExtensionInstallPrompt::ShowDialogCallback
ExtensionInstallPrompt::GetDefaultShowDialogCallback() {
  return base::Bind(&ShowExtensionInstallDialogImpl);
}
