// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab/tab_coordinator.h"

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_observer.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/clean/chrome/browser/ui/commands/tab_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_strip_commands.h"
#import "ios/clean/chrome/browser/ui/find_in_page/find_in_page_coordinator.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_coordinator.h"
#import "ios/clean/chrome/browser/ui/tab/tab_container_view_controller.h"
#import "ios/clean/chrome/browser/ui/tab/tab_navigation_controller.h"
#import "ios/clean/chrome/browser/ui/tab_strip/tab_strip_coordinator.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_coordinator.h"
#import "ios/clean/chrome/browser/ui/transitions/zoom_transition_controller.h"
#import "ios/clean/chrome/browser/ui/web_contents/web_coordinator.h"
#import "ios/shared/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/shared/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabCoordinator ()<CRWWebStateObserver,
                             TabCommands,
                             WebStateListObserving>
@property(nonatomic, strong) ZoomTransitionController* transitionController;
@property(nonatomic, strong) TabContainerViewController* viewController;
@property(nonatomic, weak) NTPCoordinator* ntpCoordinator;
@property(nonatomic, weak) WebCoordinator* webCoordinator;
@property(nonatomic, weak) ToolbarCoordinator* toolbarCoordinator;
@property(nonatomic, strong) TabNavigationController* navigationController;
@end

@implementation TabCoordinator {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<ScopedObserver<WebStateList, WebStateListObserverBridge>>
      _scopedWebStateListObserver;
}

@synthesize transitionController = _transitionController;
@synthesize presentationKey = _presentationKey;
@synthesize viewController = _viewController;
@synthesize webState = _webState;
@synthesize webCoordinator = _webCoordinator;
@synthesize ntpCoordinator = _ntpCoordinator;
@synthesize toolbarCoordinator = _toolbarCoordinator;
@synthesize navigationController = _navigationController;

#pragma mark - Public

- (void)disconnect {
  _webStateListObserver.reset();
  _webStateObserver.reset();
}

#pragma mark - BrowserCoordinator

- (void)start {
  self.viewController = [self newTabContainer];
  self.transitionController = [[ZoomTransitionController alloc] init];
  self.transitionController.presentationKey = self.presentationKey;
  self.viewController.transitioningDelegate = self.transitionController;
  self.viewController.modalPresentationStyle = UIModalPresentationCustom;
  _webStateObserver =
      base::MakeUnique<web::WebStateObserverBridge>(self.webState, self);

  _webStateListObserver = base::MakeUnique<WebStateListObserverBridge>(self);
  _scopedWebStateListObserver = base::MakeUnique<
      ScopedObserver<WebStateList, WebStateListObserverBridge>>(
      _webStateListObserver.get());
  _scopedWebStateListObserver->Add(&self.browser->web_state_list());

  [self.browser->broadcaster()
      broadcastValue:@"tabStripVisible"
            ofObject:self.viewController
            selector:@selector(broadcastTabStripVisible:)];

  CommandDispatcher* dispatcher = self.browser->dispatcher();
  // Register Commands
  [dispatcher startDispatchingToTarget:self forSelector:@selector(loadURL:)];
  [dispatcher startDispatchingToTarget:self
                           forSelector:@selector(showTabStrip)];

  // NavigationController will handle all the dispatcher navigation calls.
  self.navigationController = [[TabNavigationController alloc]
      initWithDispatcher:self.browser->dispatcher()
                webState:self.webState];

  WebCoordinator* webCoordinator = [[WebCoordinator alloc] init];
  webCoordinator.webState = self.webState;
  [self addChildCoordinator:webCoordinator];
  [webCoordinator start];
  self.webCoordinator = webCoordinator;

  ToolbarCoordinator* toolbarCoordinator = [[ToolbarCoordinator alloc] init];
  toolbarCoordinator.webState = self.webState;
  [self addChildCoordinator:toolbarCoordinator];
  [toolbarCoordinator start];
  self.toolbarCoordinator = toolbarCoordinator;

  // Create the FindInPage coordinator but do not start it.  It will be started
  // when a find in page operation is invoked.
  FindInPageCoordinator* findInPageCoordinator =
      [[FindInPageCoordinator alloc] init];
  [self addChildCoordinator:findInPageCoordinator];

  TabStripCoordinator* tabStripCoordinator = [[TabStripCoordinator alloc] init];
  [self addChildCoordinator:tabStripCoordinator];
  [tabStripCoordinator start];

  // PLACEHOLDER: Fix the order of events here. The ntpCoordinator was already
  // created above when |webCoordinator.webState = self.webState;| triggers
  // a load event, but then the webCoordinator stomps on the
  // contentViewController when it starts afterwards.
  if (self.webState->GetLastCommittedURL() == GURL(kChromeUINewTabURL)) {
    self.viewController.contentViewController =
        self.ntpCoordinator.viewController;
  }

  [super start];
}

- (void)stop {
  [super stop];
  for (BrowserCoordinator* child in self.children) {
    [self removeChildCoordinator:child];
  }
  [self.browser->broadcaster()
      stopBroadcastingForSelector:@selector(broadcastTabStripVisible:)];
  _webStateObserver.reset();
  [self.browser->dispatcher() stopDispatchingToTarget:self];
  [self.navigationController stop];
}

- (void)childCoordinatorDidStart:(BrowserCoordinator*)childCoordinator {
  if ([childCoordinator isKindOfClass:[ToolbarCoordinator class]]) {
    self.viewController.toolbarViewController = childCoordinator.viewController;
  } else if ([childCoordinator isKindOfClass:[WebCoordinator class]] ||
             [childCoordinator isKindOfClass:[NTPCoordinator class]]) {
    self.viewController.contentViewController = childCoordinator.viewController;
  } else if ([childCoordinator isKindOfClass:[TabStripCoordinator class]]) {
    self.viewController.tabStripViewController =
        childCoordinator.viewController;
  } else if ([childCoordinator isKindOfClass:[FindInPageCoordinator class]]) {
    self.viewController.findBarViewController = childCoordinator.viewController;
  }
}

- (void)childCoordinatorWillStop:(BrowserCoordinator*)childCoordinator {
  if ([childCoordinator isKindOfClass:[FindInPageCoordinator class]]) {
    self.viewController.findBarViewController = nil;
  } else if ([childCoordinator isKindOfClass:[WebCoordinator class]] ||
             [childCoordinator isKindOfClass:[NTPCoordinator class]]) {
    self.viewController.contentViewController = nil;
  }
}

- (BOOL)canAddOverlayCoordinator:(BrowserCoordinator*)overlayCoordinator {
  // This coordinator will always accept overlay coordinators.
  return YES;
}

#pragma mark - Experiment support

- (BOOL)usesBottomToolbar {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* bottomToolbarPreference =
      [defaults stringForKey:@"EnableBottomToolbar"];
  return [bottomToolbarPreference isEqualToString:@"Enabled"];
}

// Creates and returns a new view controller for use as a tab container;
// experimental configurations determine which subclass of
// TabContainerViewController to return.
- (TabContainerViewController*)newTabContainer {
  if ([self usesBottomToolbar]) {
    return [[BottomToolbarTabViewController alloc] init];
  }
  return [[TopToolbarTabViewController alloc] init];
}

#pragma mark - CRWWebStateObserver

// This will eventually be called in -didFinishNavigation and perhaps as an
// optimization in some equivalent to loadURL.
- (void)webState:(web::WebState*)webState
    didCommitNavigationWithDetails:(const web::LoadCommittedDetails&)details {
  if (webState->GetLastCommittedURL() == GURL(kChromeUINewTabURL)) {
    [self addNTPCoordinator];
  }
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [self removeNTPCoordinator];
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                 userAction:(BOOL)userAction {
  self.webState = newWebState;
  _webStateObserver =
      base::MakeUnique<web::WebStateObserverBridge>(self.webState, self);
  // Push down the new Webstate.
  self.navigationController.webState = newWebState;
  self.toolbarCoordinator.webState = newWebState;
  self.webCoordinator.webState = newWebState;

  if (self.webState->GetLastCommittedURL() == GURL(kChromeUINewTabURL)) {
    [self addNTPCoordinator];
  } else {
    [self removeNTPCoordinator];
  }
}

#pragma mark - Helper Methods

- (void)addNTPCoordinator {
  if (self.ntpCoordinator) {
    [self addChildCoordinator:self.ntpCoordinator];
  } else {
    NTPCoordinator* ntpCoordinator = [[NTPCoordinator alloc] init];
    [self addChildCoordinator:ntpCoordinator];
    [ntpCoordinator start];
    self.ntpCoordinator = ntpCoordinator;
  }
}

- (void)removeNTPCoordinator {
  if (self.ntpCoordinator) {
    [self.ntpCoordinator stop];
    [self removeChildCoordinator:self.ntpCoordinator];
    self.viewController.contentViewController =
        self.webCoordinator.viewController;
  }
}

#pragma mark - TabCommands

- (void)loadURL:(web::NavigationManager::WebLoadParams)params {
  self.webState->GetNavigationManager()->LoadURLWithParams(params);
}

#pragma mark - TabStripCommands

- (void)showTabStrip {
  self.viewController.tabStripVisible = YES;
}

@end
