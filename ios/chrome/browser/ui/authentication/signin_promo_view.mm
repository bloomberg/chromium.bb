// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_delegate.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
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
const CGFloat kStackViewVerticalSpacing = 14;
// StackView vertical padding.
const CGFloat kStackViewVerticalPadding = 20.0;
// StackView horizontal padding.
const CGFloat kStackViewHorizontalPadding = 15.0;
// Vertical padding for buttons.
const CGFloat kButtonVerticalPadding = 10;
// Image size for warm state.
const CGFloat kProfileImageFixedSize = 48;
// Size for the close button width and height.
const CGFloat kCloseButtonSize = 24;
// Padding for the close button.
const CGFloat kCloseButtonPadding = 8;
}

NSString* const kSigninPromoViewId = @"kSigninPromoViewId";
NSString* const kSigninPromoPrimaryButtonId = @"kSigninPromoPrimaryButtonId";
NSString* const kSigninPromoSecondaryButtonId =
    @"kSigninPromoSecondaryButtonId";
NSString* const kSigninPromoCloseButtonId = @"kSigninPromoCloseButtonId";

@implementation SigninPromoView {
  signin_metrics::AccessPoint _accessPoint;
}

@synthesize delegate = _delegate;
@synthesize mode = _mode;
@synthesize imageView = _imageView;
@synthesize textLabel = _textLabel;
@synthesize primaryButton = _primaryButton;
@synthesize secondaryButton = _secondaryButton;
@synthesize closeButton = _closeButton;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Set the whole element as accessible to take advantage of the
    // accessibilityCustomActions.
    self.isAccessibilityElement = YES;
    self.accessibilityIdentifier = kSigninPromoViewId;

    // Create and setup imageview that will hold the browser icon or user
    // profile
    // image.
    _imageView = [[UIImageView alloc] init];
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    _imageView.layer.masksToBounds = YES;
    _imageView.contentMode = UIViewContentModeScaleAspectFit;

    // Create and setup informative text label.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.numberOfLines = 0;
    _textLabel.textAlignment = NSTextAlignmentCenter;
    _textLabel.font = [MDCTypography buttonFont];
    _textLabel.textColor = [[MDCPalette greyPalette] tint900];

    // Create and setup primary button.
    _primaryButton = [[MDCFlatButton alloc] init];
    [_primaryButton setTitleColor:[UIColor whiteColor]
                         forState:UIControlStateNormal];
    _primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
    _primaryButton.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    _primaryButton.accessibilityIdentifier = kSigninPromoPrimaryButtonId;
    [_primaryButton addTarget:self
                       action:@selector(onPrimaryButtonAction:)
             forControlEvents:UIControlEventTouchUpInside];
    [_primaryButton setBackgroundColor:[[MDCPalette cr_bluePalette] tint500]
                              forState:UIControlStateNormal];
    _primaryButton.inkColor = [UIColor colorWithWhite:1 alpha:0.2];
    _primaryButton.contentEdgeInsets =
        UIEdgeInsetsMake(kButtonVerticalPadding, kHorizontalPadding * 2,
                         kButtonVerticalPadding, kHorizontalPadding * 2);

    // Create and setup seconday button.
    _secondaryButton = [[MDCFlatButton alloc] init];
    _secondaryButton.translatesAutoresizingMaskIntoConstraints = NO;
    _secondaryButton.accessibilityIdentifier = kSigninPromoSecondaryButtonId;
    [_secondaryButton addTarget:self
                         action:@selector(onSecondaryButtonAction:)
               forControlEvents:UIControlEventTouchUpInside];
    [_secondaryButton setTitleColor:[[MDCPalette cr_bluePalette] tint500]
                           forState:UIControlStateNormal];
    _secondaryButton.uppercaseTitle = NO;

    // Vertical stackView containing all previous view.
    UIStackView* verticalStackView =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          _imageView, _textLabel, _primaryButton, _secondaryButton
        ]];
    verticalStackView.alignment = UIStackViewAlignmentCenter;
    verticalStackView.axis = UILayoutConstraintAxisVertical;
    verticalStackView.spacing = kStackViewVerticalSpacing;
    verticalStackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:verticalStackView];

    // Create close button and adds it directly to self.
    _closeButton = [[UIButton alloc] init];
    _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
    _closeButton.accessibilityIdentifier = kSigninPromoCloseButtonId;
    [_closeButton addTarget:self
                     action:@selector(onCloseButtonAction:)
           forControlEvents:UIControlEventTouchUpInside];
    [_closeButton setImage:[UIImage imageNamed:@"signin_promo_close_gray"]
                  forState:UIControlStateNormal];
    _closeButton.hidden = YES;
    [self addSubview:_closeButton];

    [NSLayoutConstraint activateConstraints:@[
      // Add constraints for the stackView.
      [verticalStackView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kStackViewHorizontalPadding],
      [verticalStackView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kStackViewHorizontalPadding],
      [verticalStackView.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kStackViewVerticalPadding],
      [verticalStackView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-kStackViewVerticalPadding],
      // Close button constraints.
      [self.closeButton.topAnchor constraintEqualToAnchor:self.topAnchor
                                                 constant:kCloseButtonPadding],
      [self.closeButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kCloseButtonPadding],
      [self.closeButton.heightAnchor
          constraintEqualToConstant:kCloseButtonSize],
      [self.closeButton.widthAnchor constraintEqualToConstant:kCloseButtonSize],
    ]];

    _mode = SigninPromoViewModeColdState;
    [self activateColdMode];
  }
  return self;
}

- (void)prepareForReuse {
  _delegate = nil;
}

- (void)setMode:(SigninPromoViewMode)mode {
  if (mode == _mode) {
    return;
  }
  _mode = mode;
  switch (_mode) {
    case SigninPromoViewModeColdState:
      [self activateColdMode];
      return;
    case SigninPromoViewModeWarmState:
      [self activateWarmMode];
      return;
  }
  NOTREACHED();
}

- (void)activateColdMode {
  DCHECK_EQ(_mode, SigninPromoViewModeColdState);
  UIImage* logo = nil;
#if defined(GOOGLE_CHROME_BUILD)
  logo = [UIImage imageNamed:@"signin_promo_logo_chrome_color"];
#else
  logo = [UIImage imageNamed:@"signin_promo_logo_chromium_color"];
#endif  // defined(GOOGLE_CHROME_BUILD)
  DCHECK(logo);
  _imageView.image = logo;
  [_primaryButton
      setTitle:l10n_util::GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_TITLE_SIGNIN)
      forState:UIControlStateNormal];
  _secondaryButton.hidden = YES;
}

- (void)activateWarmMode {
  DCHECK_EQ(_mode, SigninPromoViewModeWarmState);
  _secondaryButton.hidden = NO;
}

- (void)setProfileImage:(UIImage*)image {
  DCHECK_EQ(SigninPromoViewModeWarmState, _mode);
  _imageView.image = CircularImageFromImage(image, kProfileImageFixedSize);
}

- (void)accessibilityPrimaryAction:(id)unused {
  [_primaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];
}

- (void)accessibilitySecondaryAction:(id)unused {
  [_secondaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];
}

- (void)accessibilityCloseAction:(id)unused {
  [_closeButton sendActionsForControlEvents:UIControlEventTouchUpInside];
}

- (CGFloat)horizontalPadding {
  return kHorizontalPadding;
}

- (void)onPrimaryButtonAction:(id)unused {
  switch (_mode) {
    case SigninPromoViewModeColdState:
      [_delegate signinPromoViewDidTapSigninWithNewAccount:self];
      break;
    case SigninPromoViewModeWarmState:
      [_delegate signinPromoViewDidTapSigninWithDefaultAccount:self];
      break;
  }
}

- (void)onSecondaryButtonAction:(id)unused {
  [_delegate signinPromoViewDidTapSigninWithOtherAccount:self];
}

- (void)onCloseButtonAction:(id)unused {
  [_delegate signinPromoViewCloseButtonWasTapped:self];
}

#pragma mark - NSObject(Accessibility)

- (NSArray<UIAccessibilityCustomAction*>*)accessibilityCustomActions {
  NSMutableArray* actions = [NSMutableArray array];

  NSString* primaryActionName =
      [_primaryButton titleForState:UIControlStateNormal];
  UIAccessibilityCustomAction* primaryCustomAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:primaryActionName
                target:self
              selector:@selector(accessibilityPrimaryAction:)];
  [actions addObject:primaryCustomAction];

  if (_mode == SigninPromoViewModeWarmState) {
    NSString* secondaryActionName =
        [_secondaryButton titleForState:UIControlStateNormal];
    UIAccessibilityCustomAction* secondaryCustomAction =
        [[UIAccessibilityCustomAction alloc]
            initWithName:secondaryActionName
                  target:self
                selector:@selector(accessibilitySecondaryAction:)];
    [actions addObject:secondaryCustomAction];
  }

  if (!_closeButton.hidden) {
    NSString* closeActionName =
        l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_CLOSE_ACCESSIBILITY);
    UIAccessibilityCustomAction* closeCustomAction =
        [[UIAccessibilityCustomAction alloc]
            initWithName:closeActionName
                  target:self
                selector:@selector(accessibilityCloseAction:)];
    [actions addObject:closeCustomAction];
  }

  return actions;
}

- (NSString*)accessibilityLabel {
  return _textLabel.text;
}

@end
