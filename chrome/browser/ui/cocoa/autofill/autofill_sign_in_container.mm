// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_sign_in_container.h"

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#include "components/autofill/browser/wallet/wallet_service_url.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

@interface AutofillSignInContainer (Private)
- (IBAction)cancelSignIn:(id)sender;
@end

@implementation AutofillSignInContainer

- (id)initWithController:(autofill::AutofillDialogController*)controller {
  if (self = [super init]) {
    controller_ = controller;
  }
  return self;
}

- (void)loadView {
  scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);
  [view setAutoresizesSubviews:YES];

  scoped_nsobject<NSButton> button([[ConstrainedWindowButton alloc] init]);
  [button setTitle:
      base::SysUTF16ToNSString(controller_->CancelSignInText())];
  [button setAction:@selector(cancelSignIn:)];
  [button setTarget:self];
  [button sizeToFit];
  [button setFrameOrigin:NSMakePoint(0, -NSHeight([button frame]))];
  [button setFrameSize:NSMakeSize(0, NSHeight([button frame]))];
  [button setAutoresizingMask:(NSViewWidthSizable | NSViewMinYMargin)];

  webContents_.reset(
      content::WebContents::Create(
          content::WebContents::CreateParams(controller_->profile())));
  NSView* webContentView = webContents_->GetView()->GetNativeView();
  [webContentView setFrameSize:NSMakeSize(0, -NSHeight([button frame]))];
  [webContentView setAutoresizingMask:
      (NSViewWidthSizable | NSViewHeightSizable)];

  [view setSubviews:@[button, webContentView]];
  self.view = view;
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

- (IBAction)cancelSignIn:(id)sender {
  controller_->EndSignInFlow();
}

@end
