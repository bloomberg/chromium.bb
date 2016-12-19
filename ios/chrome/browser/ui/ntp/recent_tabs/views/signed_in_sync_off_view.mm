// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/recent_tabs/views/signed_in_sync_off_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/fancy_ui/primary_action_button.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/views/views_utils.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/sync/sync_util.h"
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

}  // anonymous namespace

@implementation SignedInSyncOffView {
  ios::ChromeBrowserState* _browserState;  // Weak.
}

- (instancetype)initWithFrame:(CGRect)aRect
                 browserState:(ios::ChromeBrowserState*)browserState {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _browserState = browserState;
    // Create and add sign in label.
    UILabel* enableSyncLabel = recent_tabs::CreateMultilineLabel(
        l10n_util::GetNSString(IDS_IOS_OPEN_TABS_SYNC_IS_OFF_MOBILE));
    [self addSubview:enableSyncLabel];

    // Create and add sign in button.
    PrimaryActionButton* enableSyncButton = [[PrimaryActionButton alloc] init];
    [enableSyncButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    [enableSyncButton
        setTitle:l10n_util::GetNSString(IDS_IOS_OPEN_TABS_ENABLE_SYNC_MOBILE)
        forState:UIControlStateNormal];
    [enableSyncButton addTarget:self
                         action:@selector(showSyncSettings)
               forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:enableSyncButton];

    // Set constraints on label and button.
    NSDictionary* viewsDictionary = @{
      @"label" : enableSyncLabel,
      @"button" : enableSyncButton,
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
  }
  return self;
}

+ (CGFloat)desiredHeightInUITableViewCell {
  return kDesiredHeight;
}

- (void)showSyncSettings {
  [self chromeExecuteCommand:ios_internal::sync::GetSyncCommandForBrowserState(
                                 _browserState)];
}

@end
