// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/simple_signin_promo_view_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"
#include "ui/base/l10n/l10n_util.h"

@implementation SimpleSigninPromoViewMediator

@synthesize userFullName = _userFullName;
@synthesize userEmail = _userEmail;
@synthesize userImage = _userImage;

#pragma mark - SigninPromoViewConfigurator

- (void)configureSigninPromoView:(SigninPromoView*)signinPromoView {
  if (!_userEmail) {
    DCHECK(!_userFullName);
    DCHECK(!_userImage);
    signinPromoView.mode = SigninPromoViewModeColdState;
  } else {
    signinPromoView.mode = SigninPromoViewModeWarmState;
    [signinPromoView.primaryButton
        setTitle:l10n_util::GetNSStringF(
                     IDS_IOS_SIGNIN_PROMO_CONTINUE_AS,
                     base::SysNSStringToUTF16(_userFullName))
        forState:UIControlStateNormal];
    [signinPromoView.secondaryButton
        setTitle:l10n_util::GetNSStringF(IDS_IOS_SIGNIN_PROMO_NOT,
                                         base::SysNSStringToUTF16(_userEmail))
        forState:UIControlStateNormal];
    UIImage* image = _userImage;
    if (!image) {
      image = ios::GetChromeBrowserProvider()
                  ->GetSigninResourcesProvider()
                  ->GetDefaultAvatar();
    }
    [signinPromoView setProfileImage:image];
  }
}

@end
