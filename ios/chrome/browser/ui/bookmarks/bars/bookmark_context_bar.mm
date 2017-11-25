// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bars/bookmark_context_bar.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Shadow opacity for the BookmarkContextBar.
const CGFloat kShadowOpacity = 0.12f;
// Shadow radius for the BookmarkContextBar.
const CGFloat kShadowRadius = 12.0f;
// Horizontal margin for the contents of BookmarkContextBar.
const CGFloat kHorizontalMargin = 16.0f;
// Vertical margin for the contents of BookmarkContextBar.
const CGFloat kVerticalMargin = 8.0f;
// Horizontal spacing between the buttons inside BookmarkContextBar.
const CGFloat kHorizontalSpacing = 8.0f;
// Height of the toolbar in normal state.
const CGFloat kToolbarNormalHeight = 48.0f;
// Height of the expanded toolbar (buttons on multiple lines).
const CGFloat kToolbarExpandedHeight = 58.0f;
}  // namespace

@interface BookmarkContextBar ()

// Button at the leading position.
@property(nonatomic, strong) UIButton* leadingButton;
@property(nonatomic, strong) UIView* leadingButtonContainer;
// Button at the center position.
@property(nonatomic, strong) UIButton* centerButton;
@property(nonatomic, strong) UIView* centerButtonContainer;
// Button at the trailing position.
@property(nonatomic, strong) UIButton* trailingButton;
@property(nonatomic, strong) UIView* trailingButtonContainer;
// Stack view for arranging the buttons.
@property(nonatomic, strong) UIStackView* stackView;
// Height constraint for the stack view containing the buttons.
@property(nonatomic, strong) NSLayoutConstraint* heightConstraint;

@end

@implementation BookmarkContextBar
@synthesize delegate = _delegate;
@synthesize leadingButton = _leadingButton;
@synthesize leadingButtonContainer = _leadingButtonContainer;
@synthesize centerButton = _centerButton;
@synthesize centerButtonContainer = _centerButtonContainer;
@synthesize trailingButton = _trailingButton;
@synthesize trailingButtonContainer = _trailingButtonContainer;
@synthesize stackView = _stackView;
@synthesize heightConstraint = _heightConstraint;

#pragma mark - Private Methods

- (UIButton*)getButton:(ContextBarButton)contextBarButton {
  switch (contextBarButton) {
    case ContextBarLeadingButton: {
      return _leadingButton;
      break;
    }
    case ContextBarCenterButton: {
      return _centerButton;
      break;
    }
    case ContextBarTrailingButton: {
      return _trailingButton;
      break;
    }
    default: { NOTREACHED(); }
  }
}

- (void)setButtonStyle:(ContextBarButtonStyle)style
           forUIButton:(UIButton*)button {
  UIColor* textColor = style == ContextBarButtonStyleDelete
                           ? [[MDCPalette cr_redPalette] tint500]
                           : [[MDCPalette cr_bluePalette] tint500];
  UIColor* disabledColor = style == ContextBarButtonStyleDelete
                               ? [[MDCPalette cr_redPalette] tint200]
                               : [[MDCPalette cr_bluePalette] tint200];
  [button setTitleColor:textColor forState:UIControlStateNormal];
  [button setTitleColor:disabledColor forState:UIControlStateDisabled];
}

- (void)configureStyleForButton:(ContextBarButton)contextBarButton {
  UIButton* button = [self getButton:contextBarButton];
  [button setBackgroundColor:[UIColor whiteColor]];
  [button setTitleColor:[[MDCPalette cr_bluePalette] tint500]
               forState:UIControlStateNormal];
  [[button titleLabel] setFont:[MDCTypography subheadFont]];
  [self setButtonStyle:ContextBarButtonStyleDefault forUIButton:button];
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];

  button.titleLabel.numberOfLines = 2;
  button.titleLabel.adjustsFontSizeToFitWidth = YES;
  switch (contextBarButton) {
    case ContextBarLeadingButton: {
      if (UseRTLLayout()) {
        button.titleLabel.textAlignment = NSTextAlignmentRight;
        button.contentHorizontalAlignment =
            UIControlContentHorizontalAlignmentRight;
      } else {
        button.titleLabel.textAlignment = NSTextAlignmentLeft;
        button.contentHorizontalAlignment =
            UIControlContentHorizontalAlignmentLeft;
      }
      break;
    }
    case ContextBarCenterButton: {
      button.titleLabel.textAlignment = NSTextAlignmentCenter;
      button.contentHorizontalAlignment =
          UIControlContentHorizontalAlignmentCenter;
      break;
    }
    case ContextBarTrailingButton: {
      if (UseRTLLayout()) {
        button.titleLabel.textAlignment = NSTextAlignmentLeft;
        button.contentHorizontalAlignment =
            UIControlContentHorizontalAlignmentLeft;
      } else {
        button.titleLabel.textAlignment = NSTextAlignmentRight;
        button.contentHorizontalAlignment =
            UIControlContentHorizontalAlignmentRight;
      }
      break;
    }
    default: { NOTREACHED(); }
  }
}

#pragma mark - Public Methods

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    NSDictionary* views = nil;
    NSArray* constraints = nil;

    self.accessibilityIdentifier = @"context_bar";
    _leadingButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [self configureStyleForButton:ContextBarLeadingButton];
    [_leadingButton addTarget:self
                       action:@selector(leadingButtonClicked:)
             forControlEvents:UIControlEventTouchUpInside];
    _leadingButton.accessibilityIdentifier = @"context_bar_leading_button";
    _leadingButtonContainer = [[UIView alloc] init];
    _leadingButtonContainer.hidden = YES;
    [_leadingButtonContainer addSubview:_leadingButton];
    views = @{@"button" : _leadingButton};
    constraints = @[ @"V:|[button]|", @"H:|[button]" ];
    ApplyVisualConstraints(constraints, views);
    [_leadingButton.trailingAnchor
        constraintLessThanOrEqualToAnchor:_leadingButtonContainer
                                              .trailingAnchor]
        .active = YES;

    _centerButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [self configureStyleForButton:ContextBarCenterButton];
    [_centerButton addTarget:self
                      action:@selector(centerButtonClicked:)
            forControlEvents:UIControlEventTouchUpInside];
    _centerButton.accessibilityIdentifier = @"context_bar_center_button";
    _centerButtonContainer = [[UIView alloc] init];
    _centerButtonContainer.hidden = YES;
    [_centerButtonContainer addSubview:_centerButton];
    views = @{@"button" : _centerButton};
    constraints = @[ @"V:|[button]|" ];
    ApplyVisualConstraints(constraints, views);
    [_centerButton.centerXAnchor
        constraintEqualToAnchor:_centerButtonContainer.centerXAnchor]
        .active = YES;
    [_centerButton.trailingAnchor
        constraintLessThanOrEqualToAnchor:_centerButtonContainer.trailingAnchor]
        .active = YES;
    [_centerButton.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_centerButtonContainer
                                                 .leadingAnchor]
        .active = YES;

    _trailingButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [self configureStyleForButton:ContextBarTrailingButton];
    [_trailingButton addTarget:self
                        action:@selector(trailingButtonClicked:)
              forControlEvents:UIControlEventTouchUpInside];
    _trailingButton.accessibilityIdentifier = @"context_bar_trailing_button";
    _trailingButtonContainer = [[UIView alloc] init];
    _trailingButtonContainer.hidden = YES;
    [_trailingButtonContainer addSubview:_trailingButton];
    views = @{@"button" : _trailingButton};
    constraints = @[ @"V:|[button]|", @"H:[button]|" ];
    ApplyVisualConstraints(constraints, views);
    [_trailingButton.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_trailingButtonContainer
                                                 .leadingAnchor]
        .active = YES;

    _stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _leadingButtonContainer, _centerButtonContainer, _trailingButtonContainer
    ]];
    _stackView.alignment = UIStackViewAlignmentCenter;
    _stackView.distribution = UIStackViewDistributionFillEqually;
    _stackView.axis = UILayoutConstraintAxisHorizontal;

    [self addSubview:_stackView];
    _stackView.translatesAutoresizingMaskIntoConstraints = NO;
    _stackView.layoutMarginsRelativeArrangement = YES;
    PinToSafeArea(_stackView, self);
    _heightConstraint = [_stackView.heightAnchor
        constraintEqualToConstant:kToolbarNormalHeight];
    _heightConstraint.active = YES;

    _stackView.layoutMargins = UIEdgeInsetsMake(
        kVerticalMargin, kHorizontalMargin, kVerticalMargin, kHorizontalMargin);
    _stackView.spacing = kHorizontalSpacing;

    [self setBackgroundColor:[UIColor whiteColor]];
    [[self layer] setShadowOpacity:kShadowOpacity];
    [[self layer] setShadowRadius:kShadowRadius];
  }
  return self;
}

- (void)setButtonTitle:(NSString*)title forButton:(ContextBarButton)button {
  [[self getButton:button] setTitle:title forState:UIControlStateNormal];
}

- (void)setButtonStyle:(ContextBarButtonStyle)style
             forButton:(ContextBarButton)button {
  [self setButtonStyle:style forUIButton:[self getButton:button]];
}

- (void)setButtonVisibility:(BOOL)visible forButton:(ContextBarButton)button {
  // Set the visiblity of the button container.
  [self getButton:button].superview.hidden = !visible;
}

- (void)setButtonEnabled:(BOOL)enabled forButton:(ContextBarButton)button {
  [[self getButton:button] setEnabled:enabled];
}
- (void)leadingButtonClicked:(UIButton*)button {
  if (!self.delegate) {
    NOTREACHED();
  }
  [self.delegate leadingButtonClicked];
}

- (void)centerButtonClicked:(UIButton*)button {
  if (!self.delegate) {
    NOTREACHED();
  }
  [self.delegate centerButtonClicked];
}

- (void)trailingButtonClicked:(UIButton*)button {
  if (!self.delegate) {
    NOTREACHED();
  }
  [self.delegate trailingButtonClicked];
}

- (void)updateHeight {
  NSArray* buttons = @[ _leadingButton, _centerButton, _trailingButton ];

  CGFloat availableWidth = self.frame.size.width - kHorizontalMargin * 2;
  NSUInteger visibleCount = 0;

  // Count the number of visible buttons and deduct the button spacings from
  // availableWidth.
  for (UIButton* button in buttons) {
    if (!button.superview.hidden) {
      visibleCount++;
      if (visibleCount > 1) {
        availableWidth -= kHorizontalSpacing;
      }
    }
  }

  // Expand toolbar height in case word wrapping happens.
  for (UIButton* button in buttons) {
    if (!button.superview.hidden) {
      CGFloat rect = [button.titleLabel.text
                         cr_pixelAlignedSizeWithFont:button.titleLabel.font]
                         .width;
      if (rect > availableWidth / visibleCount) {
        self.heightConstraint.constant = kToolbarExpandedHeight;
        return;
      }
    }
  }

  // Use the normal height when there is no word wrapping.
  self.heightConstraint.constant = kToolbarNormalHeight;
}

@end
