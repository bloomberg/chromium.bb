// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_clear_browsing_data_coordinator.h"

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/ui/history/history_local_commands.h"
#import "ios/chrome/browser/ui/history/public/history_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_local_commands.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/web/public/referrer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HistoryClearBrowsingDataCoordinator ()

// ViewController being managed by this Coordinator.
@property(strong, nonatomic)
    TableViewNavigationController* historyClearBrowsingDataNavigationController;

@end

@implementation HistoryClearBrowsingDataCoordinator
@synthesize historyClearBrowsingDataNavigationController =
    _historyClearBrowsingDataNavigationController;
@synthesize loader = _loader;
@synthesize localDispatcher = _localDispatcher;
@synthesize presentationDelegate = _presentationDelegate;

- (void)start {
  ClearBrowsingDataTableViewController* clearBrowsingDataTableViewController =
      [[ClearBrowsingDataTableViewController alloc]
          initWithBrowserState:self.browserState];
  clearBrowsingDataTableViewController.extendedLayoutIncludesOpaqueBars = YES;
  clearBrowsingDataTableViewController.localDispatcher = self;
  // Configure and present ClearBrowsingDataNavigationController.
  self.historyClearBrowsingDataNavigationController =
      [[TableViewNavigationController alloc]
          initWithTable:clearBrowsingDataTableViewController];
  self.historyClearBrowsingDataNavigationController.toolbarHidden = NO;
  self.historyClearBrowsingDataNavigationController.modalPresentationStyle =
      UIModalPresentationFormSheet;
  self.historyClearBrowsingDataNavigationController.modalTransitionStyle =
      UIModalTransitionStyleCoverVertical;
  [self.baseViewController
      presentViewController:self.historyClearBrowsingDataNavigationController
                   animated:YES
                 completion:nil];
}

#pragma mark - ClearBrowsingDataLocalCommands

- (void)openURL:(const GURL&)URL {
  GURL copiedURL(URL);
  [self dismissClearBrowsingDataWithCompletion:^() {
    [self.localDispatcher dismissHistoryWithCompletion:^{
      [self.loader webPageOrderedOpen:copiedURL
                             referrer:web::Referrer()
                          inIncognito:NO
                         inBackground:NO
                             appendTo:kLastTab];
      [self.presentationDelegate showActiveRegularTabFromHistory];
    }];
  }];
}

- (void)dismissClearBrowsingDataWithCompletion:
    (ProceduralBlock)completionHandler {
  [self.historyClearBrowsingDataNavigationController
      dismissViewControllerAnimated:YES
                         completion:completionHandler];
  self.historyClearBrowsingDataNavigationController = nil;
}

@end
