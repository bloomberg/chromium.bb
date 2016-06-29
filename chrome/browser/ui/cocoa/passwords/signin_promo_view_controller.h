// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_SIGNIN_PROMO_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_SIGNIN_PROMO_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/passwords/base_passwords_content_view_controller.h"

// A view controller that offers user to sign in to Chrome.
@interface SignInPromoViewController
    : BasePasswordsContentViewController

- (instancetype)initWithDelegate:(id<BasePasswordsContentViewDelegate>)delegate;

@end

@interface SignInPromoViewController (Testing)
@property(readonly) NSButton* signInButton;
@property(readonly) NSButton* noButton;
@property(readonly) NSButton* closeButton;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_SIGNIN_PROMO_VIEW_CONTROLLER_H_
