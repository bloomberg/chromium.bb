// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bars/bookmark_context_bar.h"
#include "base/logging.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Shadow opacity for the BookmarkContextBar.
CGFloat kShadowOpacity = 0.2f;
// Horizontal margin for the contents of BookmarkContextBar.
CGFloat kHorizontalMargin = 8.0f;
}  // namespace

@interface BookmarkContextBar () {
}
// Button at the leading position.
@property(nonatomic, strong) UIButton* leadingButton;
// Button at the center position.
@property(nonatomic, strong) UIButton* centerButton;
// Button at the trailing position.
@property(nonatomic, strong) UIButton* trailingButton;
// Stack view for arranging the buttons.
@property(nonatomic, strong) UIStackView* stackView;

@end

@implementation BookmarkContextBar
@synthesize delegate = _delegate;
@synthesize leadingButton = _leadingButton;
@synthesize centerButton = _centerButton;
@synthesize trailingButton = _trailingButton;
@synthesize stackView = _stackView;

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
                           ? [[MDCPalette redPalette] tint500]
                           : [[MDCPalette bluePalette] tint500];
  [button setTitleColor:textColor forState:UIControlStateNormal];
}

- (void)initStyleForButton:(UIButton*)button {
  [button setBackgroundColor:[UIColor whiteColor]];
  [button setTitleColor:[[MDCPalette bluePalette] tint500]
               forState:UIControlStateNormal];
  [[button titleLabel]
      setFont:[[MDCTypography fontLoader] regularFontOfSize:14]];
  [self setButtonStyle:ContextBarButtonStyleDefault forUIButton:button];
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  button.hidden = YES;
}

#pragma mark - Public Methods

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.accessibilityIdentifier = @"Context Bar";
    _leadingButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [self initStyleForButton:_leadingButton];
    _leadingButton.contentHorizontalAlignment =
        UseRTLLayout() ? UIControlContentHorizontalAlignmentRight
                       : UIControlContentHorizontalAlignmentLeft;
    [_leadingButton addTarget:self
                       action:@selector(leadingButtonClicked:)
             forControlEvents:UIControlEventTouchUpInside];
    _leadingButton.accessibilityIdentifier = @"Context Bar Leading Button";

    _centerButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [self initStyleForButton:_centerButton];
    _centerButton.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentCenter;
    [_centerButton addTarget:self
                      action:@selector(centerButtonClicked:)
            forControlEvents:UIControlEventTouchUpInside];
    _centerButton.accessibilityIdentifier = @"Context Bar Center Button";

    _trailingButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [self initStyleForButton:_trailingButton];
    _trailingButton.contentHorizontalAlignment =
        UseRTLLayout() ? UIControlContentHorizontalAlignmentLeft
                       : UIControlContentHorizontalAlignmentRight;
    [_trailingButton addTarget:self
                        action:@selector(trailingButtonClicked:)
              forControlEvents:UIControlEventTouchUpInside];
    _trailingButton.accessibilityIdentifier = @"Context Bar Trailing Button";

    _stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _leadingButton, _centerButton, _trailingButton
    ]];
    _stackView.alignment = UIStackViewAlignmentFill;
    _stackView.distribution = UIStackViewDistributionFillEqually;
    _stackView.axis = UILayoutConstraintAxisHorizontal;

    [self addSubview:_stackView];
    _stackView.translatesAutoresizingMaskIntoConstraints = NO;
    _stackView.layoutMarginsRelativeArrangement = YES;
    [NSLayoutConstraint activateConstraints:@[
      [_stackView.layoutMarginsGuide.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kHorizontalMargin],
      [_stackView.layoutMarginsGuide.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kHorizontalMargin],
      [_stackView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_stackView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    ]];

    [self setBackgroundColor:[UIColor whiteColor]];
    [[self layer] setShadowOpacity:kShadowOpacity];
  }
  return self;
}

- (void)setButtonTitle:(NSString*)title forButton:(ContextBarButton)button {
  [[self getButton:button] setTitle:title forState:UIControlStateNormal];
  [[self getButton:button] setTitle:title forState:UIControlStateNormal];
}

- (void)setButtonStyle:(ContextBarButtonStyle)style
             forButton:(ContextBarButton)button {
  [self setButtonStyle:style forUIButton:[self getButton:button]];
}

- (void)setButtonVisibility:(BOOL)visible forButton:(ContextBarButton)button {
  [self getButton:button].hidden = !visible;
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

@end
