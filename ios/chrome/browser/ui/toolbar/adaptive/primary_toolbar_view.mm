// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive/primary_toolbar_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrimaryToolbarView ()
@property(nonatomic, strong) UIStackView* stackView;
@property(nonatomic, strong) UIView* locationBarContainer;

// Redefined as readwrite
@property(nonatomic, strong, readwrite) ToolbarButton* tabGridButton;

@end

@implementation PrimaryToolbarView

@synthesize buttonFactory = _buttonFactory;
@synthesize locationBarView = _locationBarView;
@synthesize stackView = _stackView;
@synthesize locationBarContainer = _locationBarContainer;
@synthesize tabGridButton = _tabGridButton;
@synthesize topSafeAnchor = _topSafeAnchor;

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [self setUp];
  [super willMoveToSuperview:newSuperview];
}

#pragma mark - Setup

// Sets all the subviews and constraints of this view.
- (void)setUp {
  if (self.subviews.count > 0) {
    // Setup the view only once.
    return;
  }
  DCHECK(self.buttonFactory);

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.locationBarContainer = [[UIView alloc] init];
  self.locationBarContainer.backgroundColor = [UIColor whiteColor];

  self.tabGridButton = [self.buttonFactory tabSwitcherStripButton];
  self.stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    self.tabGridButton, self.locationBarContainer
  ]];

  self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:self.stackView];

  if (@available(iOS 11, *)) {
    [NSLayoutConstraint activateConstraints:@[
      [self.stackView.leadingAnchor
          constraintEqualToAnchor:self.safeAreaLayoutGuide.leadingAnchor],
      [self.stackView.trailingAnchor
          constraintEqualToAnchor:self.safeAreaLayoutGuide.trailingAnchor],
    ]];
  } else {
    [NSLayoutConstraint activateConstraints:@[
      [self.stackView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [self.stackView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
    ]];
  }
  [NSLayoutConstraint activateConstraints:@[
    [self.stackView.topAnchor constraintEqualToAnchor:self.topSafeAnchor],
    [self.stackView.heightAnchor constraintEqualToConstant:48],
    [self.stackView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
  ]];
}

#pragma mark - Property accessors

- (void)setLocationBarView:(UIView*)locationBarView {
  if (_locationBarView == locationBarView) {
    return;
  }
  [_locationBarView removeFromSuperview];

  locationBarView.translatesAutoresizingMaskIntoConstraints = NO;
  [locationBarView setContentHuggingPriority:UILayoutPriorityDefaultLow
                                     forAxis:UILayoutConstraintAxisHorizontal];
  [self.locationBarContainer addSubview:locationBarView];
  AddSameConstraints(self.locationBarContainer, locationBarView);
  _locationBarView = locationBarView;
}

@end
