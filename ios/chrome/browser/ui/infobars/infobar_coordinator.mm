// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_coordinator.h"

#include <memory>

#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#include "ios/chrome/browser/ui/infobars/infobar_container_mediator.h"
#include "ios/chrome/browser/ui/infobars/infobar_container_view_controller.h"
#import "ios/chrome/browser/ui/infobars/infobar_positioner.h"
#import "ios/chrome/browser/ui/signin_interaction/public/signin_presenter.h"
#include "ios/chrome/browser/upgrade/upgrade_center.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarCoordinator ()<SigninPresenter>

@property(nonatomic, assign) TabModel* tabModel;

// UIViewController that contains Infobars.
@property(nonatomic, strong)
    InfobarContainerViewController* containerViewController;
// The mediator for this Coordinator.
@property(nonatomic, strong) InfobarContainerMediator* mediator;

@end

@implementation InfobarCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                                  tabModel:(TabModel*)tabModel {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    _tabModel = tabModel;
  }
  return self;
}

- (void)start {
  DCHECK(self.positioner);
  DCHECK(self.dispatcher);

  // Create and setup the ViewController.
  self.containerViewController = [[InfobarContainerViewController alloc] init];
  [self.baseViewController addChildViewController:self.containerViewController];
  // TODO(crbug.com/892376): We shouldn't modify the BaseVC hierarchy, BVC needs
  // to handle this.
  [self.baseViewController.view insertSubview:self.containerViewController.view
                                 aboveSubview:self.positioner.parentView];
  [self.containerViewController
      didMoveToParentViewController:self.baseViewController];
  self.containerViewController.positioner = self.positioner;

  // Create the mediator once the VC has been added to the View hierarchy.
  self.mediator = [[InfobarContainerMediator alloc]
      initWithConsumer:self.containerViewController
          browserState:self.browserState
              tabModel:self.tabModel];
  self.mediator.syncPresenter = self.syncPresenter;
  self.mediator.signinPresenter = self;

  [[UpgradeCenter sharedInstance] registerClient:self.mediator
                                  withDispatcher:self.dispatcher];
}

- (void)stop {
  [[UpgradeCenter sharedInstance] unregisterClient:self.mediator];
  self.mediator = nil;
}

#pragma mark - Public Interface

- (UIView*)view {
  return self.containerViewController.view;
}

- (void)updateInfobarContainer {
  [self.containerViewController updateLayoutAnimated:NO];
}

- (BOOL)isInfobarPresentingForWebState:(web::WebState*)webState {
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(webState);
  if (infoBarManager->infobar_count() > 0) {
    return YES;
  }
  return NO;
}


#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  [self.dispatcher showSignin:command
           baseViewController:self.baseViewController];
}

@end
