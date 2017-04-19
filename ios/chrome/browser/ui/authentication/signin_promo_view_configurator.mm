// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view_configurator.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SigninPromoViewConfigurator {
  NSString* _userEmail;
  NSString* _userFullName;
  UIImage* _userImage;
}

- (instancetype)initWithUserEmail:(NSString*)userEmail
                     userFullName:(NSString*)userFullName
                        userImage:(UIImage*)userImage {
  self = [super init];
  if (self) {
    DCHECK(userEmail || (!userEmail && !userFullName && !userImage));
    _userFullName = [userFullName copy];
    _userEmail = [userEmail copy];
    _userImage = [userImage copy];
  }
  return self;
}

- (void)configureSigninPromoView:(SigninPromoView*)signinPromoView {
  if (!_userEmail) {
    signinPromoView.mode = SigninPromoViewModeColdState;
  } else {
    signinPromoView.mode = SigninPromoViewModeWarmState;
    NSString* name = _userFullName.length ? _userFullName : _userEmail;
    [signinPromoView.primaryButton
        setTitle:l10n_util::GetNSStringF(IDS_IOS_SIGNIN_PROMO_CONTINUE_AS,
                                         base::SysNSStringToUTF16(name))
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
