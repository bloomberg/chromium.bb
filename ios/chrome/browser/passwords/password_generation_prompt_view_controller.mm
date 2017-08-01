// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_generation_prompt_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/passwords/password_generation_prompt_view.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Dialogs/src/MaterialDialogs.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Material Design Component constraints.
const CGFloat kMDCPadding = 24;
const CGFloat kMDCMargin = 40;

// Maximum size of the dialog.
const CGFloat kPrefWidth = 450;
const CGFloat kPrefHeight = 500;

}  // namespace

@interface PasswordGenerationPromptViewController () {
  NSString* _password;
  __weak UIViewController* _viewController;
  __weak PasswordGenerationPromptDialog* _contentView;
  MDCDialogTransitionController* _dialogTransitionController;
}

// Returns the maximum size of the dialog.
- (CGFloat)maxDialogWidth;

@end

@implementation PasswordGenerationPromptViewController

- (CGFloat)maxDialogWidth {
  CGFloat screenWidth = [[UIScreen mainScreen] bounds].size.width;
  return MIN(kPrefWidth, screenWidth - 2 * kMDCMargin);
}

- (instancetype)initWithPassword:(NSString*)password
                     contentView:(PasswordGenerationPromptDialog*)contentView
                  viewController:(UIViewController*)viewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _password = [password copy];
    _viewController = viewController;
    _contentView = contentView;
    _dialogTransitionController = [[MDCDialogTransitionController alloc] init];
    self.modalPresentationStyle = UIModalPresentationCustom;
    self.transitioningDelegate = _dialogTransitionController;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor whiteColor];

  [_contentView configureGlobalViewWithPassword:_password];
  [_contentView setTranslatesAutoresizingMaskIntoConstraints:NO];

  self.preferredContentSize = CGSizeMake([self maxDialogWidth], kPrefHeight);

  [self.view addSubview:_contentView];

  NSArray* constraints = @[
    @"V:|-(MDCPadding)-[view]", @"H:|-(MDCPadding)-[view]-(MDCPadding)-|"
  ];
  NSDictionary* viewsDictionary = @{ @"view" : _contentView };
  NSDictionary* metrics = @{ @"MDCPadding" : @(kMDCPadding) };

  ApplyVisualConstraintsWithMetricsAndOptions(
      constraints, viewsDictionary, metrics, LayoutOptionForRTLSupport());
}

- (void)viewDidLayoutSubviews {
  CGSize currentSize = CGSizeMake(
      [self maxDialogWidth], [_contentView frame].size.height + kMDCPadding);
  self.preferredContentSize = currentSize;
}

@end
