// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"

#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ToolbarButton
@synthesize visibilityMask = _visibilityMask;
@synthesize guideName = _guideName;
@synthesize hiddenInCurrentSizeClass = _hiddenInCurrentSizeClass;
@synthesize hiddenInCurrentState = _hiddenInCurrentState;

+ (instancetype)toolbarButtonWithImageForNormalState:(UIImage*)normalImage
                            imageForHighlightedState:(UIImage*)highlightedImage
                               imageForDisabledState:(UIImage*)disabledImage {
  ToolbarButton* button = [[self class] buttonWithType:UIButtonTypeCustom];
  [button setImage:normalImage forState:UIControlStateNormal];
  [button setImage:highlightedImage forState:UIControlStateHighlighted];
  [button setImage:disabledImage forState:UIControlStateDisabled];
  [button setImage:highlightedImage forState:UIControlStateSelected];
  button.titleLabel.textAlignment = NSTextAlignmentCenter;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  return button;
}

+ (instancetype)toolbarButtonWithImage:(UIImage*)image {
  ToolbarButton* button = [[self class] buttonWithType:UIButtonTypeSystem];
  [button setImage:image forState:UIControlStateNormal];
  button.titleLabel.textAlignment = NSTextAlignmentCenter;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  return button;
}

// TODO(crbug.com/800266): Remove this method as it is handled in the
// TabGridButton.
- (void)layoutSubviews {
  [super layoutSubviews];
  // If the UIButton title has text it will center it on top of the image,
  // this is currently used for the TabStripButton which displays the
  // total number of tabs.
  if (self.titleLabel.text) {
    CGSize size = self.bounds.size;
    CGPoint center = CGPointMake(size.width / 2, size.height / 2);
    self.imageView.center = center;
    self.imageView.frame = AlignRectToPixel(self.imageView.frame);
    self.titleLabel.frame = self.bounds;
  }
}

#pragma mark - Public Methods

- (void)updateHiddenInCurrentSizeClass {
  BOOL newHiddenValue = YES;

  BOOL isCompactWidth = self.traitCollection.horizontalSizeClass ==
                        UIUserInterfaceSizeClassCompact;
  BOOL isCompactHeight =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
  BOOL isRegularWidth = self.traitCollection.horizontalSizeClass ==
                        UIUserInterfaceSizeClassRegular;
  BOOL isRegularHeight =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular;

  if (isCompactWidth && isCompactHeight) {
    newHiddenValue = !(self.visibilityMask &
                       ToolbarComponentVisibilityCompactWidthCompactHeight);
  } else if (isCompactWidth && isRegularHeight) {
    newHiddenValue = !(self.visibilityMask &
                       ToolbarComponentVisibilityCompactWidthRegularHeight);
  } else if (isRegularWidth && isCompactHeight) {
    newHiddenValue = !(self.visibilityMask &
                       ToolbarComponentVisibilityRegularWidthCompactHeight);
  } else if (isRegularWidth && isRegularHeight) {
    newHiddenValue = !(self.visibilityMask &
                       ToolbarComponentVisibilityRegularWidthRegularHeight);
  }

  if (!IsIPadIdiom() &&
      self.visibilityMask & ToolbarComponentVisibilityIPhoneOnly) {
    newHiddenValue = NO;
  }
  if (newHiddenValue &&
      self.visibilityMask & ToolbarComponentVisibilityOnlyWhenEnabled) {
    newHiddenValue = !self.enabled;
  }

  if (newHiddenValue != self.hiddenInCurrentSizeClass) {
    self.hiddenInCurrentSizeClass = newHiddenValue;
    [self setHiddenForCurrentStateAndSizeClass];
  }
}

- (void)setHiddenInCurrentState:(BOOL)hiddenInCurrentState {
  _hiddenInCurrentState = hiddenInCurrentState;
  [self setHiddenForCurrentStateAndSizeClass];
}

#pragma mark - Private

// Checks if the button should be visible based on its hiddenInCurrentSizeClass
// and hiddenInCurrentState properties, then updates its visibility accordingly.
- (void)setHiddenForCurrentStateAndSizeClass {
  BOOL previouslyHidden = self.hidden;
  self.hidden = self.hiddenInCurrentState || self.hiddenInCurrentSizeClass;

  if (!self.hidden && previouslyHidden != self.hidden && self.guideName) {
    // The button is appearing. At this point, if it has a layout guide
    // associated, it should constraint it to itself.
    [NamedGuide guideWithName:self.guideName view:self].constrainedView = self;
  }
}

@end
