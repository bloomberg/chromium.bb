// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/recent_tabs/views/signed_out_view.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/fancy_ui/primary_action_button.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/views/views_utils.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Desired height of the view.
const CGFloat kDesiredHeight = 180;

}  // namespace

@implementation SignedOutView

- (instancetype)initWithFrame:(CGRect)aRect {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    // Create and add sign in label.
    UILabel* signInLabel = recent_tabs::CreateMultilineLabel(
        l10n_util::GetNSString(IDS_IOS_RECENT_TABS_SIGNIN_PROMO));
    [self addSubview:signInLabel];

    // Create and add sign in button.
    PrimaryActionButton* signInButton = [[PrimaryActionButton alloc] init];
    [signInButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    [signInButton
        setTitle:l10n_util::GetNSString(IDS_IOS_RECENT_TABS_SIGNIN_BUTTON)
        forState:UIControlStateNormal];
    [signInButton addTarget:self
                     action:@selector(showSignIn)
           forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:signInButton];

    // Set constraints on label and button.
    NSDictionary* viewsDictionary = @{
      @"label" : signInLabel,
      @"button" : signInButton,
    };

    // clang-format off
    NSArray* constraints = @[
      @"V:|-16-[label]-16-[button]-(>=16)-|",
      @"H:|-16-[label]-16-|",
      @"H:|-(>=16)-[button]-16-|"
    ];
    // clang-format on
    ApplyVisualConstraintsWithOptions(constraints, viewsDictionary,
                                      LayoutOptionForRTLSupport(), self);

    // SignedOutView is always shown directly when created, and its parent view
    // |reloadData| isn't called when the user is not signed-in.
    base::RecordAction(
        base::UserMetricsAction("Signin_Impression_FromRecentTabs"));
  }
  return self;
}

+ (CGFloat)desiredHeightInUITableViewCell {
  return kDesiredHeight;
}

- (void)showSignIn {
  base::RecordAction(base::UserMetricsAction("Signin_Signin_FromRecentTabs"));
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AUTHENTICATION_OPERATION_SIGNIN
      signInAccessPoint:signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS];
  [self chromeExecuteCommand:command];
}

@end
