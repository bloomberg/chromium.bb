// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_clear_browsing_data_coordinator.h"

#import <UIKit/UIKit.h>

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/ui/history/history_local_commands.h"
#import "ios/chrome/browser/ui/history/public/history_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_local_commands.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller_delegate.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/web/public/referrer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HistoryClearBrowsingDataCoordinator ()<
    UIViewControllerTransitioningDelegate>

// ViewController being managed by this Coordinator.
@property(strong, nonatomic)
    TableViewNavigationController* historyClearBrowsingDataNavigationController;

@end

@implementation HistoryClearBrowsingDataCoordinator
@synthesize dispatcher = _dispatcher;
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
  clearBrowsingDataTableViewController.dispatcher = self.dispatcher;
  // Configure and present ClearBrowsingDataNavigationController.
  self.historyClearBrowsingDataNavigationController =
      [[TableViewNavigationController alloc]
          initWithTable:clearBrowsingDataTableViewController];
  self.historyClearBrowsingDataNavigationController.toolbarHidden = YES;
  // Stacks on top of history "bubble" for non-compact devices.
  self.historyClearBrowsingDataNavigationController.transitioningDelegate =
      self;
  self.historyClearBrowsingDataNavigationController.modalPresentationStyle =
      UIModalPresentationCustom;
  self.historyClearBrowsingDataNavigationController.modalTransitionStyle =
      UIModalTransitionStyleCoverVertical;
  [self.baseViewController
      presentViewController:self.historyClearBrowsingDataNavigationController
                   animated:YES
                 completion:nil];
}

- (void)stopWithCompletion:(ProceduralBlock)completionHandler {
  if (self.historyClearBrowsingDataNavigationController) {
    [self.historyClearBrowsingDataNavigationController
        dismissViewControllerAnimated:YES
                           completion:completionHandler];
    self.historyClearBrowsingDataNavigationController = nil;
  } else if (completionHandler) {
    completionHandler();
  }
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
                          originPoint:CGPointZero
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

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
presentationControllerForPresentedViewController:(UIViewController*)presented
                        presentingViewController:(UIViewController*)presenting
                            sourceViewController:(UIViewController*)source {
  TableViewPresentationController* controller =
      [[TableViewPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];

  return controller;
}

@end
