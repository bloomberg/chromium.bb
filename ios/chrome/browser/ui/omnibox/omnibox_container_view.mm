// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_container_view.h"

#import "ios/chrome/browser/ui/animation_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kLeadingButtonEdgeOffset = 9;
// Offset from the leading edge to the textfield when no image is shown.
const CGFloat kTextFieldLeadingOffsetNoImage = 16;
// Space between the leading button and the textfield when a button is shown.
const CGFloat kTextFieldLeadingOffsetImage = 6;
}  // namespace

#pragma mark - OmniboxContainerView

@interface OmniboxContainerView ()
// Constraints the leading textfield side to the leading of |self|.
// Active when the |leadingView| is nil or hidden.
@property(nonatomic, strong) NSLayoutConstraint* leadingTextfieldConstraint;
// When the |leadingButton| is not hidden, this is a constraint that links the
// leading edge of the button to self leading edge. Used for animations.
@property(nonatomic, strong) NSLayoutConstraint* leadingButtonLeadingConstraint;
// The leading button, such as the security status icon.
@property(nonatomic, strong) UIButton* leadingButton;
// Redefined as readwrite.
@property(nonatomic, strong) OmniboxTextFieldIOS* textField;

@end

@implementation OmniboxContainerView
@synthesize textField = _textField;
@synthesize leadingButton = _leadingButton;
@synthesize leadingTextfieldConstraint = _leadingTextfieldConstraint;
@synthesize incognito = _incognito;
@synthesize leadingButtonLeadingConstraint = _leadingButtonLeadingConstraint;

#pragma mark - Public properties

- (void)setLeadingButton:(UIButton*)leadingButton {
  _leadingButton = leadingButton;
  _leadingButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_leadingButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_leadingButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  [_leadingButton setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisHorizontal];
  [_leadingButton setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisVertical];
}

#pragma mark - Public methods

- (instancetype)initWithFrame:(CGRect)frame
                         font:(UIFont*)font
                    textColor:(UIColor*)textColor
                    tintColor:(UIColor*)tintColor {
  self = [super initWithFrame:frame];
  if (self) {
    _textField = [[OmniboxTextFieldIOS alloc] initWithFrame:frame
                                                       font:font
                                                  textColor:textColor
                                                  tintColor:tintColor];
    [self addSubview:_textField];

    _leadingTextfieldConstraint = [_textField.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kTextFieldLeadingOffsetNoImage];

    [NSLayoutConstraint activateConstraints:@[
      [_textField.trailingAnchor
          constraintEqualToAnchor:self.layoutMarginsGuide.trailingAnchor],
      [_textField.topAnchor
          constraintEqualToAnchor:self.layoutMarginsGuide.topAnchor],
      [_textField.bottomAnchor
          constraintEqualToAnchor:self.layoutMarginsGuide.bottomAnchor],
      _leadingTextfieldConstraint,
    ]];

    _textField.translatesAutoresizingMaskIntoConstraints = NO;
    [_textField
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
  }
  return self;
}

- (void)setLeadingButtonHidden:(BOOL)hidden {
  if (!_leadingButton) {
    return;
  }

  if (hidden) {
    [_leadingButton removeFromSuperview];
    self.leadingTextfieldConstraint.active = YES;
  } else {
    [self addSubview:_leadingButton];
    self.leadingTextfieldConstraint.active = NO;
    self.leadingButtonLeadingConstraint = [self.layoutMarginsGuide.leadingAnchor
        constraintEqualToAnchor:self.leadingButton.leadingAnchor
                       constant:-kLeadingButtonEdgeOffset];

    NSLayoutConstraint* leadingButtonToTextField = nil;
    leadingButtonToTextField = [self.leadingButton.trailingAnchor
        constraintEqualToAnchor:self.textField.leadingAnchor
                       constant:-kTextFieldLeadingOffsetImage];

    [NSLayoutConstraint activateConstraints:@[
      [_leadingButton.centerYAnchor
          constraintEqualToAnchor:self.layoutMarginsGuide.centerYAnchor],
      self.leadingButtonLeadingConstraint,
      leadingButtonToTextField,
    ]];
  }
}

- (void)setLeadingButtonEnabled:(BOOL)enabled {
  _leadingButton.enabled = enabled;
}

- (void)setPlaceholderImage:(UIImage*)image {
  [self.leadingButton setImage:image forState:UIControlStateNormal];
}

#pragma mark - Private methods

// Retrieves a resource image by ID and returns it as UIImage.
- (UIImage*)placeholderImageWithId:(int)imageID {
  return [NativeImage(imageID)
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

@end
