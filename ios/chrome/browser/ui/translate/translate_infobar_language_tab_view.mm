// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/translate/translate_infobar_language_tab_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/translate/translate_infobar_language_tab_strip_view.h"
#import "ios/chrome/browser/ui/translate/translate_infobar_language_tab_view_delegate.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MaterialActivityIndicator.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Size of the activity indicator.
const CGFloat kActivityIndicatorSize = 24;

// Radius of the activity indicator.
const CGFloat kActivityIndicatorRadius = 10;

// Duration of the animation to make the activity indicator visible.
const NSTimeInterval kActivityIndicatorVisbilityAnimationDuration = 0.4;

// Title color of the action buttons in RGB.
const int kButtonTitleColor = 0x4285f4;

// Padding for contents of the button.
const CGFloat kButtonPadding = 12;

}  // namespace

@interface TranslateInfobarLanguageTabView ()

// Button subview providing the tappable area.
@property(nonatomic, weak) MDCFlatButton* button;

// Activity indicator replacing the button when in loading state.
@property(nonatomic, weak) MDCActivityIndicator* activityIndicator;

@end

@implementation TranslateInfobarLanguageTabView

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // Create and add subviews the first time this moves to a superview.
  if (newSuperview && !self.subviews.count) {
    [self setupSubviews];
  }
  [super willMoveToSuperview:newSuperview];
}

#pragma mark - Properties

- (void)setTitle:(NSString*)title {
  _title = title;
  [self.button setTitle:title forState:UIControlStateNormal];
}

- (void)setState:(TranslateInfobarLanguageTabViewState)state {
  _state = state;

  if (state == TranslateInfobarLanguageTabViewStateLoading) {
    [self.activityIndicator startAnimating];
    // Animate showing the activity indicator and hiding the button. Otherwise
    // the ripple effect on the button won't be seen.
    [UIView animateWithDuration:kActivityIndicatorVisbilityAnimationDuration
                     animations:^{
                       self.activityIndicator.hidden = NO;
                       self.button.hidden = YES;
                     }];
  } else {
    self.button.hidden = NO;
    self.activityIndicator.hidden = YES;
    [self.activityIndicator stopAnimating];

    [self.button setTitleColor:[self titleColor] forState:UIControlStateNormal];
  }
}

#pragma mark - Private

- (void)setupSubviews {
  MDCActivityIndicator* activityIndicator = [[MDCActivityIndicator alloc] init];
  self.activityIndicator = activityIndicator;
  self.activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  self.activityIndicator.cycleColors =
      @[ [[MDCPalette cr_bluePalette] tint500] ];
  [self.activityIndicator setRadius:kActivityIndicatorRadius];
  self.activityIndicator.hidden = YES;  // Initially hidden.
  [self addSubview:self.activityIndicator];

  [NSLayoutConstraint activateConstraints:@[
    [self.activityIndicator.heightAnchor
        constraintEqualToConstant:kActivityIndicatorSize],
  ]];
  AddSameCenterConstraints(self, self.activityIndicator);

  MDCFlatButton* button = [[MDCFlatButton alloc] init];
  self.button = button;
  self.button.translatesAutoresizingMaskIntoConstraints = NO;
  [self.button setUnderlyingColorHint:[UIColor blackColor]];
  self.button.contentEdgeInsets = UIEdgeInsetsMake(
      kButtonPadding, kButtonPadding, kButtonPadding, kButtonPadding);
  self.button.titleLabel.adjustsFontSizeToFitWidth = YES;
  self.button.titleLabel.minimumScaleFactor = 0.6f;
  [self.button setTitle:self.title forState:UIControlStateNormal];
  self.button.uppercaseTitle = NO;
  [self.button
      setTitleFont:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
          forState:UIControlStateNormal];
  [self.button setTitleColor:[self titleColor] forState:UIControlStateNormal];
  self.button.inkColor = [[MDCPalette greyPalette] tint300];
  [self.button addTarget:self
                  action:@selector(buttonWasTapped)
        forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button];

  AddSameConstraints(self, self.button);
}

// Returns the button's title color depending on the state.
- (UIColor*)titleColor {
  return self.state == TranslateInfobarLanguageTabViewStateSelected
             ? UIColorFromRGB(kButtonTitleColor)
             : [[MDCPalette greyPalette] tint600];
}

- (void)buttonWasTapped {
  [self.delegate translateInfobarTabViewDidTap:self];
}

@end
