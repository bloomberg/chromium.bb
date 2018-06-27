// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_steady_view.h"

#import "ios/chrome/browser/ui/location_bar/extended_touch_target_button.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Length of the trailing button side.
const CGFloat kButtonSize = 28;

// Space between the location icon and the location label.
const CGFloat kLocationImageToLabelSpacing = 2.0;

const CGFloat kButtonTrailingSpacing = 10;

// Font size used in the omnibox.
const CGFloat kFontSize = 17.0f;

}  // namespace

@interface LocationBarSteadyView ()

// The image view displaying the current location icon (i.e. http[s] status).
@property(nonatomic, strong) UIImageView* locationIconImageView;

// The view containing the location label, and (sometimes) the location image
// view.
@property(nonatomic, strong) UIView* locationContainerView;

// Constraints to hide the trailing button.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* hideButtonConstraints;

// Constraints to show the trailing button.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* showButtonConstraints;

// Constraints to hide the location image view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* hideLocationImageConstraints;

// Constraints to show the location image view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* showLocationImageConstraints;

@end

#pragma mark - LocationBarSteadyViewColorScheme

@implementation LocationBarSteadyViewColorScheme
@synthesize fontColor = _fontColor;
@synthesize placeholderColor = _placeholderColor;
@synthesize trailingButtonColor = _trailingButtonColor;

+ (instancetype)standardScheme {
  LocationBarSteadyViewColorScheme* scheme =
      [[LocationBarSteadyViewColorScheme alloc] init];

  scheme.fontColor = [UIColor colorWithWhite:0 alpha:0.7];
  scheme.placeholderColor = [UIColor colorWithWhite:0 alpha:0.3];
  scheme.trailingButtonColor = [UIColor colorWithWhite:0 alpha:0.3];

  return scheme;
}

+ (instancetype)incognitoScheme {
  LocationBarSteadyViewColorScheme* scheme =
      [[LocationBarSteadyViewColorScheme alloc] init];

  scheme.fontColor = [UIColor whiteColor];
  scheme.placeholderColor = [UIColor colorWithWhite:1 alpha:0.5];
  scheme.trailingButtonColor = [UIColor colorWithWhite:1 alpha:0.5];

  return scheme;
}

@end

#pragma mark - LocationBarSteadyView

@implementation LocationBarSteadyView
@synthesize locationLabel = _locationLabel;
@synthesize locationIconImageView = _locationIconImageView;
@synthesize trailingButton = _trailingButton;
@synthesize hideButtonConstraints = _hideButtonConstraints;
@synthesize showButtonConstraints = _showButtonConstraints;
@synthesize hideLocationImageConstraints = _hideLocationImageConstraints;
@synthesize showLocationImageConstraints = _showLocationImageConstraints;
@synthesize locationContainerView = _locationContainerView;

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _locationLabel = [[UILabel alloc] init];
    _locationIconImageView = [[UIImageView alloc] init];
    _locationIconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [_locationIconImageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    SetA11yLabelAndUiAutomationName(
        _locationIconImageView,
        IDS_IOS_PAGE_INFO_SECURITY_BUTTON_ACCESSIBILITY_LABEL,
        @"Page Security Info");
    _locationIconImageView.isAccessibilityElement = YES;

    // Setup trailing button.
    _trailingButton =
        [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
    _trailingButton.translatesAutoresizingMaskIntoConstraints = NO;

    // Setup label.
    _locationLabel.lineBreakMode = NSLineBreakByTruncatingHead;
    _locationLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [_locationLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:UILayoutConstraintAxisVertical];
    _locationLabel.font = [UIFont systemFontOfSize:kFontSize];

    // Container for location label and icon.
    _locationContainerView = [[UIView alloc] init];
    _locationContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    _locationContainerView.userInteractionEnabled = NO;
    [_locationContainerView addSubview:_locationIconImageView];
    [_locationContainerView addSubview:_locationLabel];

    [self addSubview:_trailingButton];
    [self addSubview:_locationContainerView];

    _showLocationImageConstraints = @[
      [_locationContainerView.leadingAnchor
          constraintEqualToAnchor:_locationIconImageView.leadingAnchor],
      [_locationIconImageView.trailingAnchor
          constraintEqualToAnchor:_locationLabel.leadingAnchor
                         constant:kLocationImageToLabelSpacing],
      [_locationLabel.trailingAnchor
          constraintEqualToAnchor:_locationContainerView.trailingAnchor],
      [_locationIconImageView.centerYAnchor
          constraintEqualToAnchor:_locationContainerView.centerYAnchor],
    ];

    _hideLocationImageConstraints = @[
      [_locationContainerView.leadingAnchor
          constraintEqualToAnchor:_locationLabel.leadingAnchor],
      [_locationLabel.trailingAnchor
          constraintEqualToAnchor:_locationContainerView.trailingAnchor],
    ];

    [NSLayoutConstraint activateConstraints:_showLocationImageConstraints];

    ApplyVisualConstraints(
        @[ @"V:|[label]|", @"V:|[container]|" ],
        @{@"label" : _locationLabel, @"container" : _locationContainerView});

    // Make the label graviatate towards the center of the view.
    NSLayoutConstraint* centerX = [_locationContainerView.centerXAnchor
        constraintEqualToAnchor:self.centerXAnchor];
    centerX.priority = UILayoutPriorityDefaultHigh;

    [NSLayoutConstraint activateConstraints:@[
      [_locationContainerView.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:self.leadingAnchor],
      [_trailingButton.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
      [_locationContainerView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
      [_trailingButton.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:_locationContainerView
                                                   .trailingAnchor],
      centerX,
    ]];

    // Setup hiding constraints.
    _hideButtonConstraints = @[
      [_trailingButton.widthAnchor constraintEqualToConstant:0],
      [_trailingButton.heightAnchor constraintEqualToConstant:0],
      [self.trailingButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor]
    ];

    // Setup and activate the show button constraints.
    _showButtonConstraints = @[
      // TODO(crbug.com/821804) Replace the temorary size when the icon is
      // available.
      [_trailingButton.widthAnchor constraintEqualToConstant:kButtonSize],
      [_trailingButton.heightAnchor constraintEqualToConstant:kButtonSize],
      [self.trailingButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kButtonTrailingSpacing],
    ];
    [NSLayoutConstraint activateConstraints:_showButtonConstraints];
  }
  return self;
}

- (void)setColorScheme:(LocationBarSteadyViewColorScheme*)colorScheme {
  self.trailingButton.tintColor = colorScheme.trailingButtonColor;
  self.locationLabel.textColor = colorScheme.fontColor;
  self.locationIconImageView.tintColor = colorScheme.fontColor;
}

- (void)setLocationImage:(UIImage*)locationImage {
  BOOL hadImage = self.locationIconImageView.image != nil;
  BOOL hasImage = locationImage != nil;
  self.locationIconImageView.image = locationImage;
  if (hadImage == hasImage) {
    return;
  }

  if (hasImage) {
    [self.locationContainerView addSubview:self.locationIconImageView];
    [NSLayoutConstraint
        deactivateConstraints:self.hideLocationImageConstraints];
    [NSLayoutConstraint activateConstraints:self.showLocationImageConstraints];
  } else {
    [NSLayoutConstraint
        deactivateConstraints:self.showLocationImageConstraints];
    [NSLayoutConstraint activateConstraints:self.hideLocationImageConstraints];
    [self.locationIconImageView removeFromSuperview];
  }
}

- (void)hideButton:(BOOL)hidden {
  if (hidden) {
    [NSLayoutConstraint deactivateConstraints:self.showButtonConstraints];
    [NSLayoutConstraint activateConstraints:self.hideButtonConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:self.hideButtonConstraints];
    [NSLayoutConstraint activateConstraints:self.showButtonConstraints];
  }
}

@end
