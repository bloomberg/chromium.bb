// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/tab_grid/sc_tab_grid_coordinator.h"

#import "base/format_macros.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCTabGridCoordinator ()<TabGridDataSource, TabGridActionDelegate>
@property(nonatomic, strong) TabGridViewController* viewController;
@end

@implementation SCTabGridCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize viewController = _viewController;

- (void)start {
  self.viewController = [[TabGridViewController alloc] init];
  self.viewController.title = @"Tab Grid";
  self.viewController.dataSource = self;
  self.viewController.actionDelegate = self;
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

#pragma mark - TabGridActionDelegate

- (void)showTabAtIndexPath:(NSIndexPath*)indexPath {
  [self
      showAlertWithTitle:NSStringFromProtocol(@protocol(TabGridActionDelegate))
                 message:[NSString
                             stringWithFormat:@"showTabAtIndexPath:%" PRIdNS,
                                              indexPath.item]];
}

- (void)showTabGrid {
  [self
      showAlertWithTitle:NSStringFromProtocol(@protocol(TabGridActionDelegate))
                 message:NSStringFromSelector(_cmd)];
}

- (void)showSettings {
  [self
      showAlertWithTitle:NSStringFromProtocol(@protocol(TabGridActionDelegate))
                 message:NSStringFromSelector(_cmd)];
}

#pragma mark - Private

// Helper to show simple alert.
- (void)showAlertWithTitle:(NSString*)title message:(NSString*)message {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* action =
      [UIAlertAction actionWithTitle:@"Done"
                               style:UIAlertActionStyleCancel
                             handler:nil];
  [alertController addAction:action];
  [self.baseViewController presentViewController:alertController
                                        animated:YES
                                      completion:nil];
}

@end
