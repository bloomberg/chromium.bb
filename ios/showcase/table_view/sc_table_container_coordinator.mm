// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/table_view/sc_table_container_coordinator.h"

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_container_bottom_toolbar.h"
#import "ios/chrome/browser/ui/table_view/table_container_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCTableContainerCoordinator ()
@property(nonatomic, strong) TableContainerViewController* viewController;
@end

@implementation SCTableContainerCoordinator
@synthesize viewController = _viewController;
@synthesize baseViewController = _baseViewController;

- (void)start {
  self.viewController = [[TableContainerViewController alloc]
      initWithTable:[[ChromeTableViewController alloc]
                        initWithStyle:UITableViewStylePlain]];
  self.viewController.title = @"Table View";
  TableContainerBottomToolbar* toolbar =
      [[TableContainerBottomToolbar alloc] initWithLeadingButtonText:@"Left"
                                                  trailingButtonText:@"Right"];
  [toolbar.trailingButton setTitleColor:[UIColor redColor]
                               forState:UIControlStateNormal];
  self.viewController.bottomToolbar = toolbar;
  [self.baseViewController pushViewController:self.viewController animated:YES];
}

@end
