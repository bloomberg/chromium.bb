// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button.h"

#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarButton ()
// Constraints for the named layout guide.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* namedGuideConstraints;
@end

@implementation ToolbarButton
@synthesize visibilityMask = _visibilityMask;
@synthesize guideName = _guideName;
@synthesize constraintPriority = _constraintPriority;
@synthesize hiddenInCurrentSizeClass = _hiddenInCurrentSizeClass;
@synthesize hiddenInCurrentState = _hiddenInCurrentState;
@synthesize namedGuideConstraints = _namedGuideConstraints;

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
  button.constraintPriority = UILayoutPriorityRequired;
  return button;
}

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

#pragma mark - Property accessors

- (void)setGuideName:(GuideName*)guideName {
  _guideName = guideName;
  [NSLayoutConstraint deactivateConstraints:self.namedGuideConstraints];
  self.namedGuideConstraints = nil;
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
  if (isCompactWidth &&
      (self.visibilityMask &
       ToolbarComponentVisibilityCompactWidthOnlyWhenEnabled)) {
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
  self.hidden = self.hiddenInCurrentState || self.hiddenInCurrentSizeClass;

  if (!self.namedGuideConstraints && self.guideName) {
    // The guide name can be set before the button is added to the view
    // hierarchy. Checking here if the constraints are set to prevent it.
    UILayoutGuide* guide = FindNamedGuide(_guideName, self);
    if (!guide)
      return;

    self.namedGuideConstraints = @[
      [guide.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [guide.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [guide.topAnchor constraintEqualToAnchor:self.topAnchor],
      [guide.bottomAnchor constraintEqualToAnchor:self.bottomAnchor]
    ];
    for (NSLayoutConstraint* constraint in self.namedGuideConstraints) {
      constraint.priority = self.constraintPriority;
    }
  }

  if (self.hidden) {
    [NSLayoutConstraint deactivateConstraints:self.namedGuideConstraints];
  } else {
    [NSLayoutConstraint activateConstraints:self.namedGuideConstraints];
  }
}

@end
