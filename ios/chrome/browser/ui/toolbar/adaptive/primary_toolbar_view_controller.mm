// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive/primary_toolbar_view_controller.h"

#import "ios/chrome/browser/ui/toolbar/adaptive/primary_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrimaryToolbarViewController ()

// Button factory.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;
// Redefined to be a PrimaryToolbarView.
@property(nonatomic, strong) PrimaryToolbarView* view;

@end

@implementation PrimaryToolbarViewController
@dynamic view;
@synthesize buttonFactory = _buttonFactory;
@synthesize dispatcher = _dispatcher;

#pragma mark - Public

- (instancetype)initWithButtonFactory:(ToolbarButtonFactory*)buttonFactory {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _buttonFactory = buttonFactory;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  self.view = [[PrimaryToolbarView alloc] init];
  self.view.buttonFactory = self.buttonFactory;
  if (@available(iOS 11, *)) {
    self.view.topSafeAnchor = self.view.safeAreaLayoutGuide.topAnchor;
  } else {
    self.view.topSafeAnchor = self.topLayoutGuide.bottomAnchor;
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self updateAllButtonsVisibility];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateAllButtonsVisibility];
}

#pragma mark - Property accessors

- (void)setLocationBarView:(UIView*)locationBarView {
  self.view.locationBarView = locationBarView;
}

#pragma mark - Private

// Updates all buttons visibility to match any recent WebState or SizeClass
// change.
- (void)updateAllButtonsVisibility {
  for (ToolbarButton* button in self.view.allButtons) {
    [button updateHiddenInCurrentSizeClass];
  }
}

@end
