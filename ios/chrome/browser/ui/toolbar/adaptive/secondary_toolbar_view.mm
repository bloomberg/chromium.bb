// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive/secondary_toolbar_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SecondaryToolbarView ()

// Redefined as readwrite
@property(nonatomic, strong, readwrite) NSArray<ToolbarButton*>* allButtons;

// The stack view containing the buttons.
@property(nonatomic, strong) UIStackView* stackView;

// Button to display the tools menu, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarToolsMenuButton* toolsMenuButton;
// Button to display the tab grid, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarTabGridButton* tabGridButton;
// Button to display the share menu, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* shareButton;
// Button to focus the omnibox, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* omniboxButton;
// Button to manage the bookmarks of this page, defined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* bookmarkButton;

@end

@implementation SecondaryToolbarView

@synthesize allButtons = _allButtons;
@synthesize buttonFactory = _buttonFactory;
@synthesize stackView = _stackView;
@synthesize toolsMenuButton = _toolsMenuButton;
@synthesize shareButton = _shareButton;
@synthesize omniboxButton = _omniboxButton;
@synthesize bookmarkButton = _bookmarkButton;
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

  self.backgroundColor =
      self.buttonFactory.toolbarConfiguration.backgroundColor;
  self.translatesAutoresizingMaskIntoConstraints = NO;

  self.tabGridButton = [self.buttonFactory tabGridButton];
  self.shareButton = [self.buttonFactory shareButton];
  self.omniboxButton = [self.buttonFactory omniboxButton];
  self.bookmarkButton = [self.buttonFactory bookmarkButton];
  self.toolsMenuButton = [self.buttonFactory toolsMenuButton];

  self.allButtons = @[
    self.tabGridButton, self.shareButton, self.omniboxButton,
    self.bookmarkButton, self.toolsMenuButton
  ];

  self.stackView =
      [[UIStackView alloc] initWithArrangedSubviews:self.allButtons];
  self.stackView.distribution = UIStackViewDistributionEqualSpacing;
  self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:self.stackView];

  PinToSafeArea(self.stackView, self);
}

#pragma mark - AdaptiveToolbarView

- (ToolbarButton*)backButton {
  return nil;
}

- (ToolbarButton*)forwardLeadingButton {
  return nil;
}

- (ToolbarButton*)forwardTrailingButton {
  return nil;
}

- (ToolbarButton*)stopButton {
  return nil;
}

- (ToolbarButton*)reloadButton {
  return nil;
}

- (MDCProgressView*)progressBar {
  return nil;
}

@end
