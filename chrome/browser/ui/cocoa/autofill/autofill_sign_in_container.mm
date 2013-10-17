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

@synthesize preferredSize = preferredSize_;

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
  NSView* webContentView = webContents_->GetView()->GetNativeView();
  [self setView:webContentView];
}

- (void)loadSignInPage {
  DCHECK(webContents_.get());

  // Prevent accidentaly empty |maxSize_|.
  if (NSEqualSizes(NSMakeSize(0, 0), maxSize_)) {
    maxSize_ = [[[self view] window] frame].size;
  }

  signInDelegate_.reset(
      new autofill::AutofillDialogSignInDelegate(
          dialog_, webContents_.get(),
          dialog_->delegate()->GetWebContents()->GetDelegate(),
          gfx::Size(NSSizeToCGSize(minSize_)),
          gfx::Size(NSSizeToCGSize(maxSize_))));
  webContents_->GetController().LoadURL(
      autofill::wallet::GetSignInUrl(),
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}

- (content::NavigationController*)navigationController {
  return &webContents_->GetController();
}

- (void)constrainSizeToMinimum:(NSSize)minSize maximum:(NSSize)maxSize {
  minSize_ = minSize;
  maxSize_ = maxSize;

  // Notify the web contents of its new auto-resize limits.
  if (signInDelegate_ && ![[self view] isHidden]) {
    signInDelegate_->UpdateLimitsAndEnableAutoResize(
        gfx::Size(NSSizeToCGSize(minSize_)),
        gfx::Size(NSSizeToCGSize(maxSize_)));
  }
}

- (void)setPreferredSize:(NSSize)size {
  preferredSize_ = size;

  // Always request re-layout if preferredSize changes.
  id delegate = [[[self view] window] windowController];
  if ([delegate respondsToSelector:@selector(requestRelayout)])
    [delegate performSelector:@selector(requestRelayout)];
}

@end
