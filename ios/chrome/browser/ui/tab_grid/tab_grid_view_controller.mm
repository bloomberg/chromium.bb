// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#import "ios/chrome/browser/ui/tab_grid/grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_top_toolbar.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridViewController ()
@property(nonatomic, weak) GridViewController* regularTabsViewController;
@property(nonatomic, weak) GridViewController* incognitoTabsViewController;
@end

@implementation TabGridViewController
@synthesize regularTabsViewController = _regularTabsViewController;
@synthesize incognitoTabsViewController = _incognitoTabsViewController;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self setupRegularTabsViewController];
  [self setupToolbars];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

#pragma mark - Public

- (id<GridConsumer>)regularTabsConsumer {
  return self.regularTabsViewController;
}

- (id<GridConsumer>)incognitoTabsConsumer {
  return self.incognitoTabsViewController;
}

#pragma mark - Private

// Adds the regular tabs GridViewController as a contained view controller, and
// sets constraints.
- (void)setupRegularTabsViewController {
  GridViewController* viewController = [[GridViewController alloc] init];
  viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:viewController];
  [self.view addSubview:viewController.view];
  [viewController didMoveToParentViewController:self];
  self.regularTabsViewController = viewController;

  NSArray* constraints = @[
    [viewController.view.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [viewController.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [viewController.view.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [viewController.view.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

// Adds the top and bottom toolbars and sets constraints.
- (void)setupToolbars {
  UIView* topToolbar = [[TabGridTopToolbar alloc] init];
  topToolbar.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* bottomToolbar = [[TabGridBottomToolbar alloc] init];
  bottomToolbar.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:topToolbar];
  [self.view addSubview:bottomToolbar];

  NSMutableArray* constraints = [NSMutableArray
      arrayWithObjects:[topToolbar.topAnchor
                           constraintEqualToAnchor:self.view.topAnchor],
                       [topToolbar.leadingAnchor
                           constraintEqualToAnchor:self.view.leadingAnchor],
                       [topToolbar.trailingAnchor
                           constraintEqualToAnchor:self.view.trailingAnchor],
                       [bottomToolbar.bottomAnchor
                           constraintEqualToAnchor:self.view.bottomAnchor],
                       [bottomToolbar.leadingAnchor
                           constraintEqualToAnchor:self.view.leadingAnchor],
                       [bottomToolbar.trailingAnchor
                           constraintEqualToAnchor:self.view.trailingAnchor],
                       nil];
  if (@available(iOS 11, *)) {
    // SafeArea is only available in iOS  11+.
    [constraints addObjectsFromArray:@[
      [topToolbar.bottomAnchor
          constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                         constant:topToolbar.intrinsicContentSize.height],
      [bottomToolbar.topAnchor
          constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                         constant:-bottomToolbar.intrinsicContentSize.height],
    ]];
  } else {
    // Top and bottom layout guides are deprecated starting in iOS 11.
    [constraints addObjectsFromArray:@[
      [topToolbar.bottomAnchor
          constraintEqualToAnchor:self.topLayoutGuide.bottomAnchor
                         constant:topToolbar.intrinsicContentSize.height],
      [bottomToolbar.topAnchor
          constraintEqualToAnchor:self.bottomLayoutGuide.topAnchor
                         constant:-bottomToolbar.intrinsicContentSize.height],
    ]];
  }
  [NSLayoutConstraint activateConstraints:constraints];

  // The content inset of the tab grids must be modified so that the toolbars do
  // not obscure the tabs.
  self.regularTabsViewController.gridView.contentInset =
      UIEdgeInsetsMake(topToolbar.intrinsicContentSize.height, 0,
                       bottomToolbar.intrinsicContentSize.height, 0);
}

#pragma mark - TabGridTransitionStateProvider properties

- (BOOL)selectedTabVisible {
  return NO;
}

@end
