// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_delegate.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#include "ios/chrome/browser/ui/ui_util.h"
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
// Spacing within stackView.
const CGFloat kLegacySubViewVerticalSpacing = 14;
// StackView vertical padding.
const CGFloat kLegacyStackViewVerticalPadding = 20.0;
// StackView horizontal padding.
const CGFloat kLegacyStackViewHorizontalPadding = 15.0;
// Vertical padding for buttons.
const CGFloat kButtonVerticalPadding = 10;
// Image size for warm state.
const CGFloat kProfileImageFixedSize = 48;
// Size for the close button width and height.
const CGFloat kCloseButtonSize = 24;
// Padding for the close button.
const CGFloat kCloseButtonPadding = 8;

// UI Refresh Constants:
// Text label gray color.
const CGFloat kGrayHexColor = 0x6d6d72;
// Action button blue background color.
const CGFloat kBlueHexColor = 0x1A73E8;
// Vertical spacing between stackView and cell contentView.
const CGFloat kStackViewVerticalPadding = 11.0;
// Horizontal spacing between stackView and cell contentView.
const CGFloat kStackViewHorizontalPadding = 16.0;
// Spacing within stackView.
const CGFloat kStackViewSubViewSpacing = 13.0;
// Horizontal Inset between button contents and edge.
const CGFloat kButtonTitleHorizontalContentInset = 40.0;
// Vertical Inset between button contents and edge.
const CGFloat kButtonTitleVerticalContentInset = 8.0;
// Button corner radius.
const CGFloat kButtonCornerRadius = 8;
// Trailing margin for the close button.
const CGFloat kCloseButtonTrailingMargin = 5;
// Size for the close button width and height.
const CGFloat kCloseButtonWidthHeight = 24;
// Size for the imageView width and height.
const CGFloat kImageViewWidthHeight = 32;
}

NSString* const kSigninPromoViewId = @"kSigninPromoViewId";
NSString* const kSigninPromoPrimaryButtonId = @"kSigninPromoPrimaryButtonId";
NSString* const kSigninPromoSecondaryButtonId =
    @"kSigninPromoSecondaryButtonId";
NSString* const kSigninPromoCloseButtonId = @"kSigninPromoCloseButtonId";

@interface SigninPromoView ()
// Re-declare as readwrite.
@property(nonatomic, readwrite) UIImageView* imageView;
@property(nonatomic, readwrite) UILabel* textLabel;
@property(nonatomic, readwrite) UIButton* primaryButton;
@property(nonatomic, readwrite) UIButton* secondaryButton;
@property(nonatomic, readwrite) UIButton* closeButton;
@property(nonatomic, assign) BOOL loadRefreshUI;
@end

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
@synthesize loadRefreshUI = _loadRefreshUI;

- (instancetype)initWithFrame:(CGRect)frame
                        style:(SigninPromoViewUI)signinPromoViewUI {
  self = [super initWithFrame:frame];
  if (self) {
    _loadRefreshUI = (IsUIRefreshPhase1Enabled() &&
                      signinPromoViewUI == SigninPromoViewUIRefresh);
  }
  return self;
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0))
    return;

  [self setupUI];

  // Default mode.
  self.mode = SigninPromoViewModeColdState;
  [self activateColdMode];
}

- (void)setupUI {
  // Set the whole element as accessible to take advantage of the
  // accessibilityCustomActions.
  self.isAccessibilityElement = YES;
  self.accessibilityIdentifier = kSigninPromoViewId;

  // Create and setup imageview that will hold the browser icon or user profile
  // image.
  self.imageView = [[UIImageView alloc] init];
  self.imageView.translatesAutoresizingMaskIntoConstraints = NO;
  self.imageView.layer.masksToBounds = YES;
  self.imageView.contentMode = UIViewContentModeScaleAspectFit;

  // Create and setup informative text label.
  self.textLabel = [[UILabel alloc] init];
  self.textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.textLabel.numberOfLines = 0;
  self.textLabel.textAlignment = NSTextAlignmentCenter;
  if (self.loadRefreshUI) {
    self.textLabel.lineBreakMode = NSLineBreakByWordWrapping;
    self.textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    self.textLabel.textColor = UIColorFromRGB(kGrayHexColor);
  } else {
    _textLabel.font = [MDCTypography buttonFont];
    _textLabel.textColor = [[MDCPalette greyPalette] tint900];
  }

  // Create and setup primary button.
  UIButton* primaryButton;
  UIEdgeInsets primaryButtonInsets;
  if (self.loadRefreshUI) {
    primaryButton = [[UIButton alloc] init];
    primaryButton.backgroundColor = UIColorFromRGB(kBlueHexColor);
    [primaryButton.titleLabel
        setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]];
    primaryButton.layer.cornerRadius = kButtonCornerRadius;
    primaryButton.clipsToBounds = YES;
    primaryButtonInsets = UIEdgeInsetsMake(
        kButtonTitleVerticalContentInset, kButtonTitleHorizontalContentInset,
        kButtonTitleVerticalContentInset, kButtonTitleHorizontalContentInset);
  } else {
    primaryButton = [[MDCFlatButton alloc] init];
    MDCFlatButton* materialButton =
        base::mac::ObjCCastStrict<MDCFlatButton>(primaryButton);
    [materialButton setBackgroundColor:[[MDCPalette cr_bluePalette] tint500]
                              forState:UIControlStateNormal];
    materialButton.inkColor = [UIColor colorWithWhite:1 alpha:0.2];
    primaryButtonInsets =
        UIEdgeInsetsMake(kButtonVerticalPadding, kHorizontalPadding * 2,
                         kButtonVerticalPadding, kHorizontalPadding * 2);
  }
  self.primaryButton = primaryButton;
  DCHECK(self.primaryButton);
  self.primaryButton.accessibilityIdentifier = kSigninPromoPrimaryButtonId;
  [self.primaryButton setTitleColor:[UIColor whiteColor]
                           forState:UIControlStateNormal];
  self.primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
  self.primaryButton.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  [self.primaryButton addTarget:self
                         action:@selector(onPrimaryButtonAction:)
               forControlEvents:UIControlEventTouchUpInside];
  self.primaryButton.contentEdgeInsets = primaryButtonInsets;

  // Create and setup seconday button.
  UIButton* secondaryButton;
  if (self.loadRefreshUI) {
    secondaryButton = [[UIButton alloc] init];
    [secondaryButton.titleLabel
        setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]];
    [secondaryButton setTitleColor:UIColorFromRGB(kBlueHexColor)
                          forState:UIControlStateNormal];
  } else {
    secondaryButton = [[MDCFlatButton alloc] init];
    MDCFlatButton* materialButton =
        base::mac::ObjCCastStrict<MDCFlatButton>(secondaryButton);
    materialButton.uppercaseTitle = NO;
    [materialButton setTitleColor:[[MDCPalette cr_bluePalette] tint500]
                         forState:UIControlStateNormal];
  }
  self.secondaryButton = secondaryButton;
  DCHECK(self.secondaryButton);
  self.secondaryButton.translatesAutoresizingMaskIntoConstraints = NO;
  self.secondaryButton.accessibilityIdentifier = kSigninPromoSecondaryButtonId;
  [self.secondaryButton addTarget:self
                           action:@selector(onSecondaryButtonAction:)
                 forControlEvents:UIControlEventTouchUpInside];

  // Vertical stackView containing all previous view.
  UIStackView* verticalStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        self.imageView, self.textLabel, self.primaryButton, self.secondaryButton
      ]];
  verticalStackView.alignment = UIStackViewAlignmentCenter;
  verticalStackView.axis = UILayoutConstraintAxisVertical;
  verticalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  verticalStackView.spacing = self.loadRefreshUI
                                  ? kStackViewSubViewSpacing
                                  : kLegacySubViewVerticalSpacing;
  [self addSubview:verticalStackView];

  // Create close button and adds it directly to self.
  self.closeButton = [[UIButton alloc] init];
  self.closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  self.closeButton.accessibilityIdentifier = kSigninPromoCloseButtonId;
  [self.closeButton addTarget:self
                       action:@selector(onCloseButtonAction:)
             forControlEvents:UIControlEventTouchUpInside];
  [self.closeButton setImage:[UIImage imageNamed:@"signin_promo_close_gray"]
                    forState:UIControlStateNormal];
  self.closeButton.hidden = YES;
  [self addSubview:self.closeButton];

  // Add legacys or UIRefresh constraints for the stackView.
  if (self.loadRefreshUI) {
    [NSLayoutConstraint activateConstraints:@[
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
      [self.imageView.heightAnchor
          constraintEqualToConstant:kImageViewWidthHeight],
      [self.imageView.widthAnchor
          constraintEqualToConstant:kImageViewWidthHeight],
      // Close button constraints.
      [self.closeButton.topAnchor constraintEqualToAnchor:self.topAnchor],
      [self.closeButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:kCloseButtonTrailingMargin],
      [self.closeButton.heightAnchor
          constraintEqualToConstant:kCloseButtonWidthHeight],
      [self.closeButton.widthAnchor
          constraintEqualToConstant:kCloseButtonWidthHeight],
    ]];
  } else {
    [NSLayoutConstraint activateConstraints:@[
      [verticalStackView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kLegacyStackViewHorizontalPadding],
      [verticalStackView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kLegacyStackViewHorizontalPadding],
      [verticalStackView.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kLegacyStackViewVerticalPadding],
      [verticalStackView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-kLegacyStackViewVerticalPadding],
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
  }
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
  self.imageView.image = logo;
  [self.primaryButton
      setTitle:l10n_util::GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_TITLE_SIGNIN)
      forState:UIControlStateNormal];
  self.secondaryButton.hidden = YES;
}

- (void)activateWarmMode {
  DCHECK_EQ(_mode, SigninPromoViewModeWarmState);
  self.secondaryButton.hidden = NO;
}

- (void)setProfileImage:(UIImage*)image {
  DCHECK_EQ(SigninPromoViewModeWarmState, _mode);
  self.imageView.image = CircularImageFromImage(image, kProfileImageFixedSize);
}

- (void)accessibilityPrimaryAction:(id)unused {
  [self.primaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];
}

- (void)accessibilitySecondaryAction:(id)unused {
  [self.secondaryButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
}

- (void)accessibilityCloseAction:(id)unused {
  [self.closeButton sendActionsForControlEvents:UIControlEventTouchUpInside];
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
      [self.primaryButton titleForState:UIControlStateNormal];
  UIAccessibilityCustomAction* primaryCustomAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:primaryActionName
                target:self
              selector:@selector(accessibilityPrimaryAction:)];
  [actions addObject:primaryCustomAction];

  if (_mode == SigninPromoViewModeWarmState) {
    NSString* secondaryActionName =
        [self.secondaryButton titleForState:UIControlStateNormal];
    UIAccessibilityCustomAction* secondaryCustomAction =
        [[UIAccessibilityCustomAction alloc]
            initWithName:secondaryActionName
                  target:self
                selector:@selector(accessibilitySecondaryAction:)];
    [actions addObject:secondaryCustomAction];
  }

  if (!self.closeButton.hidden) {
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
  return self.textLabel.text;
}

@end
