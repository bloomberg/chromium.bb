// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/notify_auto_signin_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr int kBackgroundColor = 0x4285F4;
constexpr int kViewHeight = 48;

}  // namespace

@implementation NotifyUserAutoSigninViewController

@synthesize username = _username;

- (instancetype)initWithUsername:(NSString*)username {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _username = username;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = UIColorFromRGB(kBackgroundColor);
  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  // Blue background view for notification.
  UIView* contentView = [[UIView alloc] init];
  contentView.translatesAutoresizingMaskIntoConstraints = NO;
  contentView.userInteractionEnabled = NO;

  // View containing "Signing in as ..." text.
  UILabel* textView = [[UILabel alloc] init];
  textView.translatesAutoresizingMaskIntoConstraints = NO;
  UIFont* font = [MDCTypography body1Font];
  textView.text =
      l10n_util::GetNSStringF(IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TITLE,
                              base::SysNSStringToUTF16(_username));
  textView.font = font;
  textView.textColor = [UIColor whiteColor];
  textView.userInteractionEnabled = NO;

  // Add subiews
  [contentView addSubview:textView];
  [self.view addSubview:contentView];

  // Text view must leave 48pt on the left for user's avatar. Set the
  // constraints.
  NSDictionary* childrenViewsDictionary = @{
    @"textView" : textView,
  };
  NSArray* childrenConstraints = @[
    @"V:|[textView]|",
    @"H:|-48-[textView]-12-|",
  ];
  ApplyVisualConstraints(childrenConstraints, childrenViewsDictionary);

  [contentView.heightAnchor constraintEqualToConstant:kViewHeight].active = YES;
  PinToSafeArea(contentView, self.view);
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (parent == nil) {
    return;
  }

  // Set constraints for blue background.
  [NSLayoutConstraint activateConstraints:@[
    [self.view.bottomAnchor
        constraintEqualToAnchor:self.view.superview.bottomAnchor],
    [self.view.leadingAnchor
        constraintEqualToAnchor:self.view.superview.leadingAnchor],
    [self.view.trailingAnchor
        constraintEqualToAnchor:self.view.superview.trailingAnchor],
  ]];
}

@end
