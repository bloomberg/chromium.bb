// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/signin_promo_item.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Horizontal padding for label and buttons.
const CGFloat kHorizontalPadding = 40;
// Vertical padding for the image and the label.
const CGFloat kVerticalPadding = 12;
// Vertical padding for buttons.
const CGFloat kButtonVerticalPadding = 6;
// Image size.
const CGFloat kImageFixedSize = 48;
// Button height.
const CGFloat kButtonHeight = 36;
}

@implementation SigninPromoItem

@synthesize profileImage = _profileImage;
@synthesize profileName = _profileName;
@synthesize profileEmail = _profileEmail;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SigninPromoCell class];
  }
  return self;
}

#pragma mark - CollectionViewItem

- (void)configureCell:(SigninPromoCell*)cell {
  [super configureCell:cell];
  cell.imageView.image = _profileImage;
  cell.textLabel.text = l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_SETTINGS);
  [cell.signinButton
      setTitle:l10n_util::GetNSStringF(IDS_IOS_SIGNIN_PROMO_CONTINUE_AS,
                                       base::SysNSStringToUTF16(_profileName))
      forState:UIControlStateNormal];
  [cell.notMeButton
      setTitle:l10n_util::GetNSStringF(IDS_IOS_SIGNIN_PROMO_NOT,
                                       base::SysNSStringToUTF16(_profileEmail))
      forState:UIControlStateNormal];
}

@end

@implementation SigninPromoCell

@synthesize imageView = _imageView;
@synthesize textLabel = _textLabel;
@synthesize signinButton = _signinButton;
@synthesize notMeButton = _notMeButton;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    [self addSubviews];
    [self setDefaultViewStyling];
    [self setViewConstraints];
  }
  return self;
}

// Create and add subviews.
- (void)addSubviews {
  UIView* contentView = self.contentView;
  contentView.clipsToBounds = YES;

  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_imageView];

  _textLabel = [[UILabel alloc] init];
  _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_textLabel];

  _signinButton = [[MDCFlatButton alloc] init];
  _signinButton.translatesAutoresizingMaskIntoConstraints = NO;
  _signinButton.accessibilityIdentifier = @"signin_promo_button";
  [contentView addSubview:_signinButton];

  _notMeButton = [[MDCFlatButton alloc] init];
  _notMeButton.translatesAutoresizingMaskIntoConstraints = NO;
  _notMeButton.accessibilityIdentifier = @"signin_promo_no_button";
  [contentView addSubview:_notMeButton];
}

- (void)setDefaultViewStyling {
  _imageView.contentMode = UIViewContentModeCenter;
  _imageView.layer.masksToBounds = YES;
  _imageView.contentMode = UIViewContentModeScaleAspectFit;
  _textLabel.font = [MDCTypography buttonFont];
  _textLabel.textColor = [[MDCPalette greyPalette] tint900];
  _textLabel.numberOfLines = 0;
  _textLabel.textAlignment = NSTextAlignmentCenter;
  [_signinButton setBackgroundColor:[[MDCPalette cr_bluePalette] tint500]
                           forState:UIControlStateNormal];
  _signinButton.customTitleColor = [UIColor whiteColor];
  _signinButton.inkColor = [UIColor colorWithWhite:1 alpha:0.2];
  _notMeButton.customTitleColor = [[MDCPalette cr_bluePalette] tint500];
  _notMeButton.uppercaseTitle = NO;
}

- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  [NSLayoutConstraint activateConstraints:@[
    // Set vertical anchors.
    [_imageView.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                         constant:kVerticalPadding * 2],
    [_textLabel.topAnchor constraintEqualToAnchor:_imageView.bottomAnchor
                                         constant:kVerticalPadding],
    [_signinButton.topAnchor
        constraintEqualToAnchor:_textLabel.bottomAnchor
                       constant:kVerticalPadding + kButtonVerticalPadding],
    [_notMeButton.topAnchor constraintEqualToAnchor:_signinButton.bottomAnchor
                                           constant:kButtonVerticalPadding * 2],
    [contentView.bottomAnchor
        constraintEqualToAnchor:_notMeButton.bottomAnchor
                       constant:kVerticalPadding + kButtonVerticalPadding],

    // Set horizontal anchors.
    [_imageView.centerXAnchor
        constraintEqualToAnchor:contentView.centerXAnchor],
    [_textLabel.centerXAnchor
        constraintEqualToAnchor:contentView.centerXAnchor],
    [_signinButton.centerXAnchor
        constraintEqualToAnchor:contentView.centerXAnchor],
    [_signinButton.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kHorizontalPadding],
    [contentView.trailingAnchor
        constraintEqualToAnchor:_signinButton.trailingAnchor
                       constant:kHorizontalPadding],
    [_notMeButton.centerXAnchor
        constraintEqualToAnchor:contentView.centerXAnchor],
    [_notMeButton.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kHorizontalPadding],
    [contentView.trailingAnchor
        constraintEqualToAnchor:_notMeButton.trailingAnchor
                       constant:kHorizontalPadding],

    // Fix width and height.
    [_imageView.widthAnchor constraintEqualToConstant:kImageFixedSize],
    [_imageView.heightAnchor constraintEqualToConstant:kImageFixedSize],
    [_signinButton.heightAnchor constraintEqualToConstant:kButtonHeight],
    [_notMeButton.heightAnchor constraintEqualToConstant:kButtonHeight],
  ]];
}

// Implements -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.bounds);
  _textLabel.preferredMaxLayoutWidth = parentWidth - 2 * kHorizontalPadding;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

#pragma mark - Action

- (void)accessibilitySigninAction:(id)unused {
  // TODO(jlebel): Need to implement
  NSLog(@"Sign-in action");
}

- (void)accessibilityNotMeAction:(id)unused {
  // TODO(jlebel): Need to implement
  NSLog(@"Not me action");
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  _imageView.image = nil;
  _textLabel.text = nil;
  _signinButton = nil;
  _notMeButton = nil;
}

#pragma mark - NSObject(Accessibility)

- (NSArray<UIAccessibilityCustomAction*>*)accessibilityCustomActions {
  NSString* signinActionName =
      [_signinButton titleForState:UIControlStateNormal];
  UIAccessibilityCustomAction* signinCustomAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:signinActionName
                target:self
              selector:@selector(accessibilitySigninAction:)];
  NSString* notMeActionName = [_notMeButton titleForState:UIControlStateNormal];
  UIAccessibilityCustomAction* notMeCustomAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:notMeActionName
                target:self
              selector:@selector(accessibilityNotMeAction:)];
  return [NSArray arrayWithObjects:signinCustomAction, notMeCustomAction, nil];
}

- (NSString*)accessibilityLabel {
  return l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_SETTINGS);
}

@end
