// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/history/history_coordinator.h"

#include <memory>

#include "components/browser_sync/profile_sync_service.h"
#include "components/history/core/browser/browsing_history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/history/history_table_container_view_controller.h"
#include "ios/chrome/browser/ui/history/history_table_view_controller.h"
#import "ios/chrome/browser/ui/history/history_transitioning_delegate.h"
#include "ios/chrome/browser/ui/history/ios_browsing_history_driver.h"
#import "ios/chrome/browser/ui/table_view/table_container_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HistoryCoordinator () {
  // Provides dependencies and funnels callbacks from BrowsingHistoryService.
  std::unique_ptr<IOSBrowsingHistoryDriver> _browsingHistoryDriver;
  // Abstraction to communicate with HistoryService and WebHistoryService.
  std::unique_ptr<history::BrowsingHistoryService> _browsingHistoryService;
}
// ViewController being managed by this Coordinator.
@property(nonatomic, strong)
    HistoryTableContainerViewController* historyContainerViewController;

// The transitioning delegate used by the history view controller.
@property(nonatomic, strong)
    HistoryTransitioningDelegate* historyTransitioningDelegate;
@end

@implementation HistoryCoordinator
@synthesize dispatcher = _dispatcher;
@synthesize historyContainerViewController = _historyContainerViewController;
@synthesize historyTransitioningDelegate = _historyTransitioningDelegate;
@synthesize loader = _loader;

- (void)start {
  // Initialize and configure HistoryTableViewController.
  HistoryTableViewController* historyTableViewController =
      [[HistoryTableViewController alloc] init];
  historyTableViewController.browserState = self.browserState;
  historyTableViewController.loader = self.loader;

  // Adds the "Done" button and hooks it up to |stop|.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(stop)];
  [dismissButton setAccessibilityIdentifier:kTableContainerDismissButtonId];
  historyTableViewController.navigationItem.rightBarButtonItem = dismissButton;

  // Initialize and configure HistoryServices.
  _browsingHistoryDriver = std::make_unique<IOSBrowsingHistoryDriver>(
      self.browserState, historyTableViewController);
  _browsingHistoryService = std::make_unique<history::BrowsingHistoryService>(
      _browsingHistoryDriver.get(),
      ios::HistoryServiceFactory::GetForBrowserState(
          self.browserState, ServiceAccessType::EXPLICIT_ACCESS),
      IOSChromeProfileSyncServiceFactory::GetForBrowserState(
          self.browserState));
  historyTableViewController.historyService = _browsingHistoryService.get();

  // Initialize and configure HistoryContainerViewController.

  // Present HistoryContainerViewController.
  self.historyContainerViewController =
      [[HistoryTableContainerViewController alloc]
          initWithTable:historyTableViewController];
  self.historyContainerViewController.dispatcher = self.dispatcher;
  self.historyContainerViewController.toolbarHidden = NO;
  historyTableViewController.delegate = self.historyContainerViewController;

  self.historyTransitioningDelegate =
      [[HistoryTransitioningDelegate alloc] init];
  self.historyContainerViewController.transitioningDelegate =
      self.historyTransitioningDelegate;
  [self.historyContainerViewController
      setModalPresentationStyle:UIModalPresentationCustom];
  [self.baseViewController
      presentViewController:self.historyContainerViewController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  [self.historyContainerViewController dismissViewControllerAnimated:YES
                                                          completion:nil];
  self.historyContainerViewController = nil;
}

@end
