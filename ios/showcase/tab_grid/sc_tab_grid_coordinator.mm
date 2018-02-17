// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/tab_grid/sc_tab_grid_coordinator.h"

#import "ios/chrome/browser/ui/tab_grid/grid_item.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCTabGridCoordinator ()<UINavigationControllerDelegate>
@property(nonatomic, strong) TabGridViewController* viewController;
@end

@implementation SCTabGridCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize viewController = _viewController;

- (void)start {
  self.viewController = [[TabGridViewController alloc] init];
  self.viewController.title = @"Full TabGrid UI";
  self.baseViewController.delegate = self;
  self.baseViewController.hidesBarsOnSwipe = YES;
  [self.baseViewController pushViewController:self.viewController animated:YES];
}

#pragma mark - UINavigationControllerDelegate

// This delegate method is used as a way to have a completion handler after
// pushing onto a navigation controller.
- (void)navigationController:(UINavigationController*)navigationController
       didShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  if (viewController != self.viewController)
    return;

  NSMutableArray* items = [[NSMutableArray alloc] init];
  for (int i = 0; i < 10; i++) {
    GridItem* item = [[GridItem alloc] init];
    item.title = @"The New York Times - Breaking News";
    [items addObject:item];
  }
  [self.viewController.regularTabsConsumer populateItems:items selectedIndex:0];
}

@end
