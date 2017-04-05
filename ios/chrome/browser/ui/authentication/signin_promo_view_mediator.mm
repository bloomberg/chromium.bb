// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninPromoViewMediator ()<ChromeIdentityServiceObserver>
@end

@implementation SigninPromoViewMediator {
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;
  UIImage* _identityAvatar;
}

@synthesize delegate = _delegate;
@synthesize defaultIdentity = _defaultIdentity;

- (instancetype)init {
  self = [super init];
  if (self) {
    NSArray* identities = ios::GetChromeBrowserProvider()
                              ->GetChromeIdentityService()
                              ->GetAllIdentitiesSortedForDisplay();
    if (identities.count != 0) {
      [self selectIdentity:identities[0]];
    }
    _identityServiceObserver =
        base::MakeUnique<ChromeIdentityServiceObserverBridge>(self);
  }
  return self;
}

- (void)selectIdentity:(ChromeIdentity*)identity {
  _defaultIdentity = identity;
  if (!_defaultIdentity) {
    _identityAvatar = nil;
  } else {
    __weak SigninPromoViewMediator* weakSelf = self;
    ios::GetChromeBrowserProvider()
        ->GetChromeIdentityService()
        ->GetAvatarForIdentity(identity, ^(UIImage* identityAvatar) {
          if (weakSelf.defaultIdentity != identity) {
            return;
          }
          [weakSelf identityAvatarUpdated:identityAvatar];
        });
  }
}

- (void)identityAvatarUpdated:(UIImage*)identityAvatar {
  _identityAvatar = identityAvatar;
  [_delegate signinPromoViewMediatorCurrentIdentityUpdated:self];
}

#pragma mark - SigninPromoViewConfigurator

- (void)configureSigninPromoView:(SigninPromoView*)signinPromoView {
  if (!_defaultIdentity) {
    signinPromoView.mode = SigninPromoViewModeColdState;
  } else {
    signinPromoView.mode = SigninPromoViewModeWarmState;
    NSString* userEmail = _defaultIdentity.userEmail;
    [signinPromoView.secondaryButton
        setTitle:l10n_util::GetNSStringF(IDS_IOS_SIGNIN_PROMO_NOT,
                                         base::SysNSStringToUTF16(userEmail))
        forState:UIControlStateNormal];
    NSString* userFullName = _defaultIdentity.userFullName;
    if (!userFullName) {
      userFullName = userEmail;
    }
    [signinPromoView.primaryButton
        setTitle:l10n_util::GetNSStringF(IDS_IOS_SIGNIN_PROMO_CONTINUE_AS,
                                         base::SysNSStringToUTF16(userFullName))
        forState:UIControlStateNormal];
    UIImage* image = _identityAvatar;
    if (!image) {
      image = ios::GetChromeBrowserProvider()
                  ->GetSigninResourcesProvider()
                  ->GetDefaultAvatar();
    }
    [signinPromoView setProfileImage:image];
  }
}

#pragma mark - ChromeIdentityServiceObserver

- (void)onIdentityListChanged {
  ChromeIdentity* newIdentity = nil;
  NSArray* identities = ios::GetChromeBrowserProvider()
                            ->GetChromeIdentityService()
                            ->GetAllIdentitiesSortedForDisplay();
  if (identities.count != 0) {
    newIdentity = identities[0];
  }
  if (newIdentity != _defaultIdentity) {
    [self selectIdentity:newIdentity];
    [_delegate signinPromoViewMediatorCurrentIdentityChanged:self];
  }
}

- (void)onProfileUpdate:(ChromeIdentity*)identity {
  if (identity == _defaultIdentity) {
    [_delegate signinPromoViewMediatorCurrentIdentityUpdated:self];
  }
}

@end
