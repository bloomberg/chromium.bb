// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/password_reuse_warning_dialog_cocoa.h"

#include "chrome/browser/ui/cocoa/browser_dialogs_views_mac.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/password_reuse_warning_view_controller.h"
#include "ui/base/material_design/material_design_controller.h"

namespace safe_browsing {

void ShowPasswordReuseModalWarningDialog(
    content::WebContents* web_contents,
    ChromePasswordProtectionService* service,
    OnWarningDone done_callback) {
  DCHECK(web_contents);

  if (ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    chrome::ShowPasswordReuseWarningDialog(web_contents, service,
                                           std::move(done_callback));
    return;
  }

  // Dialog owns itself.
  new PasswordReuseWarningDialogCocoa(web_contents, service,
                                      std::move(done_callback));
}

}  // namespace safe_browsing

PasswordReuseWarningDialogCocoa::PasswordReuseWarningDialogCocoa(
    content::WebContents* web_contents,
    safe_browsing::ChromePasswordProtectionService* service,
    safe_browsing::OnWarningDone callback)
    : service_(service),
      url_(web_contents->GetLastCommittedURL()),
      callback_(std::move(callback)) {
  controller_.reset(
      [[PasswordReuseWarningViewController alloc] initWithOwner:this]);

  // Setup the constrained window that will show the view.
  base::scoped_nsobject<NSWindow> window([[ConstrainedWindowCustomWindow alloc]
      initWithContentRect:[[controller_ view] bounds]]);
  [[window contentView] addSubview:[controller_ view]];
  base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc] initWithCustomWindow:window]);
  window_ = CreateAndShowWebModalDialogMac(this, web_contents, sheet);

  if (service_)
    service_->AddObserver(this);
}

PasswordReuseWarningDialogCocoa::~PasswordReuseWarningDialogCocoa() {
  if (service_)
    service_->RemoveObserver(this);
}

void PasswordReuseWarningDialogCocoa::OnStartingGaiaPasswordChange() {
  window_->CloseWebContentsModalDialog();
}

void PasswordReuseWarningDialogCocoa::OnGaiaPasswordChanged() {
  window_->CloseWebContentsModalDialog();
}

void PasswordReuseWarningDialogCocoa::OnMarkingSiteAsLegitimate(
    const GURL& url) {
  if (url_.GetWithEmptyPath() == url.GetWithEmptyPath())
    window_->CloseWebContentsModalDialog();
}

void PasswordReuseWarningDialogCocoa::InvokeActionForTesting(
    safe_browsing::ChromePasswordProtectionService::WarningAction action) {
  switch (action) {
    case safe_browsing::ChromePasswordProtectionService::CHANGE_PASSWORD:
      OnChangePassword();
      break;
    case safe_browsing::ChromePasswordProtectionService::IGNORE_WARNING:
      OnIgnore();
      break;
    case safe_browsing::ChromePasswordProtectionService::CLOSE:
      window_->CloseWebContentsModalDialog();
      break;
    default:
      NOTREACHED();
      break;
  }
}

safe_browsing::ChromePasswordProtectionService::WarningUIType
PasswordReuseWarningDialogCocoa::GetObserverType() {
  return safe_browsing::ChromePasswordProtectionService::MODAL_DIALOG;
}

void PasswordReuseWarningDialogCocoa::OnChangePassword() {
  std::move(callback_).Run(
      safe_browsing::PasswordProtectionService::CHANGE_PASSWORD);
  window_->CloseWebContentsModalDialog();
}

void PasswordReuseWarningDialogCocoa::OnIgnore() {
  std::move(callback_).Run(
      safe_browsing::PasswordProtectionService::IGNORE_WARNING);
  window_->CloseWebContentsModalDialog();
}

void PasswordReuseWarningDialogCocoa::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  if (callback_)
    std::move(callback_).Run(safe_browsing::PasswordProtectionService::CLOSE);
}
