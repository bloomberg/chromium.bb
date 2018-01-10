// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive/secondary_toolbar_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SecondaryToolbarView ()

// The stack view containing the buttons.
@property(nonatomic, strong) UIStackView* stackView;

// Redefined as readwrite
@property(nonatomic, strong, readwrite) ToolbarToolsMenuButton* toolsMenuButton;
@property(nonatomic, strong, readwrite) ToolbarButton* tabGridButton;
@property(nonatomic, strong, readwrite) ToolbarButton* shareButton;
@property(nonatomic, strong, readwrite) ToolbarButton* omniboxButton;
@property(nonatomic, strong, readwrite) ToolbarButton* bookmarksButton;

@end

@implementation SecondaryToolbarView

@synthesize buttonFactory = _buttonFactory;
@synthesize stackView = _stackView;
@synthesize toolsMenuButton = _toolsMenuButton;
@synthesize shareButton = _shareButton;
@synthesize omniboxButton = _omniboxButton;
@synthesize bookmarksButton = _bookmarksButton;
@synthesize tabGridButton = _tabGridButton;

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [self setUp];
  [super willMoveToSuperview:newSuperview];
}

#pragma mark - Setup

// Sets all the subviews and constraints of this view.
- (void)setUp {
  if (self.subviews.count > 0) {
    // Make sure the view is instantiated only once.
    return;
  }
  DCHECK(self.buttonFactory);

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.backgroundColor = [UIColor whiteColor];

  self.tabGridButton = [self.buttonFactory tabSwitcherStripButton];
  self.shareButton = [self.buttonFactory shareButton];
  self.omniboxButton = [self.buttonFactory omniboxButton];
  self.bookmarksButton = [self.buttonFactory bookmarkButton];
  self.toolsMenuButton = [self.buttonFactory toolsMenuButton];

  self.stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    self.tabGridButton, self.shareButton, self.omniboxButton,
    self.bookmarksButton, self.toolsMenuButton
  ]];
  self.stackView.distribution = UIStackViewDistributionEqualSpacing;
  self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:self.stackView];

  PinToSafeArea(self.stackView, self);
}

@end
