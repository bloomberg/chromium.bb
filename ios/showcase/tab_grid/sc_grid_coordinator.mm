// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/tab_grid/sc_grid_coordinator.h"

#import "ios/chrome/browser/ui/tab_grid/grid_item.h"
#import "ios/chrome/browser/ui/tab_grid/grid_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCGridCoordinator ()
@end

@implementation SCGridCoordinator
@synthesize baseViewController = _baseViewController;

- (void)start {
  GridViewController* gridViewController = [[GridViewController alloc] init];
  GridItem* item1 = [[GridItem alloc] init];
  item1.title = @"Basecamp: project with an attitude.";
  GridItem* item2 = [[GridItem alloc] init];
  item2.title = @"Basecamp: project with an attitude.";
  GridItem* item3 = [[GridItem alloc] init];
  item3.title = @"Basecamp: project with an attitude.";
  NSArray* items = @[ item1, item2, item3 ];
  [gridViewController populateItems:items selectedIndex:1];
  gridViewController.title = @"Grid UI";
  [self.baseViewController pushViewController:gridViewController animated:YES];
}

@end
