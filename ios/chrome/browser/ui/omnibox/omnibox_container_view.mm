// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_container_view.h"

#import "ios/chrome/browser/ui/animation_util.h"
#import "ios/chrome/browser/ui/omnibox/clipping_textfield_container.h"
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

@interface OmniboxTextFieldIOS ()

// Gets the bounds of the rect covering the URL.
- (CGRect)preEditLabelRectForBounds:(CGRect)bounds;
// Creates the UILabel if it doesn't already exist and adds it as a
// subview.
- (void)createSelectionViewIfNecessary;
// Helper method used to set the text of this field.  Updates the selection view
// to contain the correct inline autocomplete text.
- (void)setTextInternal:(NSAttributedString*)text
     autocompleteLength:(NSUInteger)autocompleteLength;
// Override deleteBackward so that backspace can clear query refinement chips.
- (void)deleteBackward;
// Returns the layers affected by animations added by |-animateFadeWithStyle:|.
- (NSArray*)fadeAnimationLayers;
// Returns the text that is displayed in the field, including any inline
// autocomplete text that may be present as an NSString. Returns the same
// value as -|displayedText| but prefer to use this to avoid unnecessary
// conversion from NSString to base::string16 if possible.
- (NSString*)nsDisplayedText;

@end

#pragma mark - OmniboxContainerView

@interface OmniboxContainerView ()
// Constraints the leading textfield side to the leading of |self|.
// Active when the |leadingView| is nil or hidden.
@property(nonatomic, strong) NSLayoutConstraint* leadingTextfieldConstraint;
// When the |leadingButton| is not hidden, this is a constraint that links the
// leading edge of the button to self leading edge. Used for animations.
@property(nonatomic, strong) NSLayoutConstraint* leadingButtonLeadingConstraint;
// The textfield container. The |textField| is contained in it, and its frame
// should not be managed directly, instead the location bar uses this container.
// This is required to achieve desired text clipping of long URLs.
@property(nonatomic, strong) ClippingTextFieldContainer* textFieldContainer;
@end

@implementation OmniboxContainerView
@synthesize textField = _textField;
@synthesize leadingButton = _leadingButton;
@synthesize leadingTextfieldConstraint = _leadingTextfieldConstraint;
@synthesize incognito = _incognito;
@synthesize leadingButtonLeadingConstraint = _leadingButtonLeadingConstraint;
@synthesize textFieldContainer = _textFieldContainer;

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

    // The text field is put into a container.

    // TODO(crbug.com/789968): remove these insets when the location bar
    // background is managed by this view and not toolbar controller. These
    // insets allow the gradient masking of the omnibox to not extend beyond
    // the omnibox background's visible frame.
    self.layoutMargins = UIEdgeInsetsMake(3, 3, 3, 3);

    _textFieldContainer = [[ClippingTextFieldContainer alloc]
        initWithClippingTextField:_textField];
    [self addSubview:_textFieldContainer];

    _leadingTextfieldConstraint = [_textFieldContainer.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kTextFieldLeadingOffsetNoImage];

    [NSLayoutConstraint activateConstraints:@[
      [_textFieldContainer.trailingAnchor
          constraintEqualToAnchor:self.layoutMarginsGuide.trailingAnchor],
      [_textFieldContainer.topAnchor
          constraintEqualToAnchor:self.layoutMarginsGuide.topAnchor],
      [_textFieldContainer.bottomAnchor
          constraintEqualToAnchor:self.layoutMarginsGuide.bottomAnchor],
      _leadingTextfieldConstraint,
    ]];

    _textFieldContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [_textFieldContainer
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
        constraintEqualToAnchor:self.textFieldContainer.leadingAnchor
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
