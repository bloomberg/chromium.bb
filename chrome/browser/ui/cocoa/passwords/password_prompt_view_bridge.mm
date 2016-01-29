// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/passwords/password_prompt_view_bridge.h"

#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/passwords/account_chooser_view_controller.h"
#include "chrome/browser/ui/passwords/password_dialog_controller.h"
#include "content/public/browser/web_contents.h"

PasswordPromptViewBridge::PasswordPromptViewBridge(
    PasswordDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller),
      web_contents_(web_contents) {
}

PasswordPromptViewBridge::~PasswordPromptViewBridge() {
  [view_controller_ setBridge:nil];
}

void PasswordPromptViewBridge::ShowAccountChooser() {
  view_controller_.reset(
      [[AccountChooserViewController alloc] initWithBridge:this]);
  // Setup the constrained window that will show the view.
  base::scoped_nsobject<NSWindow> window([[ConstrainedWindowCustomWindow alloc]
      initWithContentRect:[[view_controller_ view] bounds]]);
  [window setContentView:[view_controller_ view]];
  base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc] initWithCustomWindow:window]);
  constrained_window_.reset(
      new ConstrainedWindowMac(this, web_contents_, sheet));
}

void PasswordPromptViewBridge::ControllerGone() {
  controller_ = nullptr;
  PerformClose();
}

void PasswordPromptViewBridge::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  if (controller_)
    controller_->OnCloseAccountChooser();
  delete this;
}

void PasswordPromptViewBridge::PerformClose() {
  constrained_window_->CloseWebContentsModalDialog();
}

PasswordDialogController*
PasswordPromptViewBridge::GetDialogController() {
  return controller_;
}

net::URLRequestContextGetter*
PasswordPromptViewBridge::GetRequestContext() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext())->
      GetRequestContext();
}

AccountChooserPrompt* CreateAccountChooserPromptView(
    PasswordDialogController* controller, content::WebContents* web_contents) {
  return new PasswordPromptViewBridge(controller, web_contents);
}

AutoSigninFirstRunPrompt* CreateAutoSigninPromptView(
    PasswordDialogController* controller, content::WebContents* web_contents) {
  // TODO(vasilii): return PasswordPromptViewBridge.
  return nullptr;
}

