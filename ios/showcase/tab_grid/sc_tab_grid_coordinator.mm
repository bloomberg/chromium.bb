// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/tab_grid/sc_tab_grid_coordinator.h"

#import "base/format_macros.h"
#import "ios/clean/chrome/browser/ui/commands/tab_commands.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"
#import "ios/showcase/common/protocol_alerter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCTabGridCoordinator ()<TabGridDataSource>
@property(nonatomic, strong) TabGridViewController* viewController;
@property(nonatomic, strong) ProtocolAlerter* alerter;
@end

@implementation SCTabGridCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize viewController = _viewController;
@synthesize alerter = _alerter;

- (void)start {
  self.alerter =
      [[ProtocolAlerter alloc] initWithProtocols:@[ @protocol(TabCommands) ]];
  self.alerter.baseViewController = self.baseViewController;

  self.viewController = [[TabGridViewController alloc] init];
  self.viewController.title = @"Tab Grid";
  self.viewController.dataSource = self;
  self.viewController.tabCommandHandler =
      static_cast<id<TabCommands>>(self.alerter);

  [self.baseViewController setHidesBarsOnSwipe:YES];
  [self.baseViewController pushViewController:self.viewController animated:YES];
}

#pragma mark - TabGridDataSource

- (NSUInteger)numberOfTabsInTabGrid {
  return 3;
}

- (NSString*)titleAtIndex:(NSInteger)index {
  return [NSString stringWithFormat:@"Tab %" PRIdNS, index];
}

- (NSInteger)indexOfActiveTab {
  return NSNotFound;
}

@end
