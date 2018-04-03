// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/table_container_view_controller.h"

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_container_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TableContainerViewController ()
// Change dismissButton to readwrite.
@property(nonatomic, strong, readwrite) UIBarButtonItem* dismissButton;
@end

@implementation TableContainerViewController
@synthesize bottomToolbar = _bottomToolbar;
@synthesize dismissButton = _dismissButton;
@synthesize tableViewController = _tableViewController;

#pragma mark - Public Interface

- (instancetype)initWithTable:(ChromeTableViewController*)table {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    self.tableViewController = table;
    // Dismiss Button configuration.
    _dismissButton = [[UIBarButtonItem alloc] init];
    _dismissButton.style = UIBarButtonItemStylePlain;
    _dismissButton.title =
        l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
    [_dismissButton setAccessibilityIdentifier:kTableContainerDismissButtonId];
  }
  return self;
}

#pragma mark Setters and Getters

// TODO(crbug.com/805178): Temporary Toolbar code for prototyping purposes.
- (void)setBottomToolbar:(UIView*)bottomToolbar {
  _bottomToolbar = bottomToolbar;
  _bottomToolbar.backgroundColor = [UIColor lightGrayColor];
  _bottomToolbar.translatesAutoresizingMaskIntoConstraints = NO;
  _bottomToolbar.alpha = 0.8;
}

#pragma mark - View Lifecycle

- (void)viewDidLoad {
  // If the NavigationBar is not translucid we need to set
  // |self.extendedLayoutIncludesOpaqueBars| to YES in order to avoid a top
  // margin inset on the |_tableViewController| subview.
  if (!self.navigationController.navigationBar.isTranslucent) {
    self.extendedLayoutIncludesOpaqueBars = YES;
  }

  // NavigationController
  if (@available(iOS 11, *)) {
    self.navigationController.navigationBar.prefersLargeTitles = YES;
  }

  // TableView
  self.tableViewController.tableView.translatesAutoresizingMaskIntoConstraints =
      NO;
  [self addChildViewController:self.tableViewController];
  [self.view addSubview:self.tableViewController.view];
  [self.tableViewController didMoveToParentViewController:self];

  // Add Bottom Toolbar to view hierarchy if it exists.
  if (self.bottomToolbar) {
    // Add the Bottom Toolbar after the TableView since it needs to be on top of
    // the view hierarchy.
    [self.view addSubview:_bottomToolbar];
    [self setUpBottomToolbarConstraints];
  }

  [self setUpConstraints];
}

#pragma mark - UI Setup

- (void)setUpConstraints {
  NSArray* constraints = @[
    [self.tableViewController.view.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [self.tableViewController.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.tableViewController.view.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.tableViewController.view.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

- (void)setUpBottomToolbarConstraints {
  NSArray* constraints = @[
    [self.bottomToolbar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.bottomToolbar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.bottomToolbar.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.bottomToolbar.heightAnchor constraintEqualToConstant:40],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

@end
