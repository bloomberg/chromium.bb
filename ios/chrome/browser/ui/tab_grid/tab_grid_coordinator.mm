// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_coordinator.h"

#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/main/bvc_container_view_controller.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_adaptor.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_transition_handler.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridCoordinator ()
// Commad dispatcher used while this coordinator's view controller is active.
// (for compatibility with the TabSwitcher protocol).
@property(nonatomic, strong) CommandDispatcher* dispatcher;
// Object that internally backs the public  TabSwitcher
@property(nonatomic, strong) TabGridAdaptor* adaptor;
// Container view controller for the BVC to live in; this class's view
// controller will present this.
@property(nonatomic, strong) BVCContainerViewController* bvcContainer;
// Transitioning delegate for the view controller.
@property(nonatomic, strong) TabGridTransitionHandler* transitionHandler;
// Mediator for regular Tabs.
@property(nonatomic, strong) TabGridMediator* regularTabsMediator;
// Mediator for incognito Tabs.
@property(nonatomic, strong) TabGridMediator* incognitoTabsMediator;
@end

@implementation TabGridCoordinator
// Superclass property.
@synthesize mainViewController = _mainViewController;
// Public properties.
@synthesize animationsDisabledForTesting = _animationsDisabledForTesting;
@synthesize regularTabModel = _regularTabModel;
@synthesize incognitoTabModel = _incognitoTabModel;
// Private properties.
@synthesize dispatcher = _dispatcher;
@synthesize adaptor = _adaptor;
@synthesize bvcContainer = _bvcContainer;
@synthesize transitionHandler = _transitionHandler;
@synthesize regularTabsMediator = _regularTabsMediator;
@synthesize incognitoTabsMediator = _incognitoTabsMediator;

- (instancetype)initWithWindow:(nullable UIWindow*)window
    applicationCommandEndpoint:
        (id<ApplicationCommands>)applicationCommandEndpoint {
  if ((self = [super initWithWindow:window])) {
    _dispatcher = [[CommandDispatcher alloc] init];
    [_dispatcher startDispatchingToTarget:self
                              forProtocol:@protocol(BrowserCommands)];
    [_dispatcher startDispatchingToTarget:applicationCommandEndpoint
                              forProtocol:@protocol(ApplicationCommands)];
  }
  return self;
}

#pragma mark - Public properties

- (id<TabSwitcher>)tabSwitcher {
  return self.adaptor;
}

#pragma mark - MainCoordinator properties

- (id<ViewControllerSwapping>)viewControllerSwapper {
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  TabGridViewController* mainViewController =
      [[TabGridViewController alloc] init];
  self.transitionHandler = [[TabGridTransitionHandler alloc] init];
  self.transitionHandler.provider = mainViewController;
  mainViewController.transitioningDelegate = self.transitionHandler;
  _mainViewController = mainViewController;
  self.window.rootViewController = self.mainViewController;
  self.adaptor = [[TabGridAdaptor alloc] init];
  self.adaptor.tabGridViewController = self.mainViewController;
  self.adaptor.adaptedDispatcher =
      static_cast<id<ApplicationCommands, BrowserCommands, OmniboxFocuser,
                     ToolbarCommands>>(self.dispatcher);
}

- (void)stop {
  [self.dispatcher stopDispatchingForProtocol:@protocol(BrowserCommands)];
  [self.dispatcher stopDispatchingForProtocol:@protocol(ApplicationCommands)];
}

#pragma mark - ViewControllerSwapping

- (UIViewController*)activeViewController {
  if (self.bvcContainer) {
    DCHECK_EQ(self.bvcContainer,
              self.mainViewController.presentedViewController);
    DCHECK(self.bvcContainer.currentBVC);
    return self.bvcContainer.currentBVC;
  }
  return self.mainViewController;
}

- (UIViewController*)viewController {
  return self.mainViewController;
}

- (void)showTabSwitcher:(id<TabSwitcher>)tabSwitcher
             completion:(ProceduralBlock)completion {
  DCHECK(tabSwitcher);
  DCHECK_EQ([tabSwitcher viewController], self.mainViewController);
  // It's also expected that |tabSwitcher| will be |self.tabSwitcher|, but that
  // may not be worth a DCHECK?

  // If a BVC is currently being presented, dismiss it.  This will trigger any
  // necessary animations.
  if (self.bvcContainer) {
    self.bvcContainer.transitioningDelegate = self.transitionHandler;
    self.bvcContainer = nil;
    BOOL animated = !self.animationsDisabledForTesting;
    [self.mainViewController dismissViewControllerAnimated:animated
                                                completion:completion];
  } else {
    if (completion) {
      completion();
    }
  }
}

- (void)showTabViewController:(UIViewController*)viewController
                   completion:(ProceduralBlock)completion {
  DCHECK(viewController);

  // If another BVC is already being presented, swap this one into the
  // container.
  if (self.bvcContainer) {
    self.bvcContainer.currentBVC = viewController;
    if (completion) {
      completion();
    }
    return;
  }

  self.bvcContainer = [[BVCContainerViewController alloc] init];
  self.bvcContainer.currentBVC = viewController;
  self.bvcContainer.transitioningDelegate = self.transitionHandler;
  BOOL animated = !self.animationsDisabledForTesting;

  // Extened |completion| to also signal the tab switcher delegate
  // that the animated "tab switcher dismissal" (that is, presenting something
  // on top of the tab switcher) transition has completed.
  ProceduralBlock extendedCompletion = ^{
    [self.tabSwitcher.delegate
        tabSwitcherDismissTransitionDidEnd:self.tabSwitcher];
    if (completion) {
      completion();
    }
  };

  [self.mainViewController presentViewController:self.bvcContainer
                                        animated:animated
                                      completion:extendedCompletion];
}

#pragma mark - BrowserCommands

- (void)openNewTab:(OpenNewTabCommand*)command {
}

- (void)closeAllTabs {
}

- (void)closeAllIncognitoTabs {
}

@end
