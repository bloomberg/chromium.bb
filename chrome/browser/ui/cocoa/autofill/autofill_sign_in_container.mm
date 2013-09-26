// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_sign_in_container.h"

#import <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_sign_in_delegate.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_cocoa.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

@implementation AutofillSignInContainer

- (id)initWithDialog:(autofill::AutofillDialogCocoa*)dialog {
  if (self = [super init]) {
    dialog_ = dialog;
  }
  return self;
}

- (void)loadView {
  webContents_.reset(
      content::WebContents::Create(
          content::WebContents::CreateParams(dialog_->delegate()->profile())));
  signInDelegate_.reset(
      new autofill::AutofillDialogSignInDelegate(
          dialog_, webContents_.get(),
          dialog_->delegate()->GetWebContents()->GetDelegate(),
          // TODO(groby): Implement proper minimum and maximum sizing on Mac.
          dialog_->GetSize(), dialog_->GetSize()));
  NSView* webContentView = webContents_->GetView()->GetNativeView();
  [self setView:webContentView];
}

- (void)loadSignInPage {
  DCHECK(webContents_.get());
  webContents_->GetController().LoadURL(
      autofill::wallet::GetSignInUrl(),
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}

- (content::NavigationController*)navigationController {
  return &webContents_->GetController();
}

@end
