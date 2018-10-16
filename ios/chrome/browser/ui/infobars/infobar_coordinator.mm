// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_coordinator.h"

#include <memory>

#include "base/ios/block_types.h"
#include "ios/chrome/browser/infobars/infobar_container_delegate_ios.h"
#include "ios/chrome/browser/infobars/infobar_container_ios.h"
#include "ios/chrome/browser/infobars/infobar_container_view.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/ui/authentication/re_signin_infobar_delegate.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/infobars/infobar_positioner.h"
#import "ios/chrome/browser/ui/settings/sync_utils/sync_util.h"
#import "ios/chrome/browser/ui/signin_interaction/public/signin_presenter.h"
#include "ios/chrome/browser/upgrade/upgrade_center.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarCoordinator ()<InfobarContainerStateDelegate,
                                 TabModelObserver,
                                 SigninPresenter> {
  // Bridge class to deliver container change notifications.
  std::unique_ptr<InfoBarContainerDelegateIOS> _infoBarContainerDelegate;

  // A single infobar container handles all infobars in all tabs. It keeps
  // track of infobars for current tab (accessed via infobar helper of
  // the current tab).
  std::unique_ptr<InfoBarContainerIOS> _infoBarContainer;
}
@property(nonatomic, assign) TabModel* tabModel;

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
    [_tabModel addObserver:self];
    _infoBarContainerDelegate.reset(new InfoBarContainerDelegateIOS(self));
    _infoBarContainer.reset(
        new InfoBarContainerIOS(_infoBarContainerDelegate.get()));
  }
  return self;
}

- (void)start {
  [self.baseViewController.view insertSubview:_infoBarContainer->view()
                                 aboveSubview:self.positioner.parentView];
  CGRect infoBarFrame = self.positioner.parentView.frame;
  infoBarFrame.origin.y = CGRectGetMaxY(infoBarFrame);
  infoBarFrame.size.height = 0;
  [_infoBarContainer->view() setFrame:infoBarFrame];

  infobars::InfoBarManager* infoBarManager = nullptr;
  if (self.tabModel.currentTab) {
    DCHECK(self.tabModel.currentTab.webState);
    infoBarManager =
        InfoBarManagerImpl::FromWebState(self.tabModel.currentTab.webState);
  }
  _infoBarContainer->ChangeInfoBarManager(infoBarManager);
}

- (void)stop {
  [self.tabModel removeObserver:self];
}

#pragma mark - Public Interface

- (UIView*)view {
  return _infoBarContainer->view();
}

- (void)restoreInfobars {
  _infoBarContainer->RestoreInfobars();
}

- (void)suspendInfobars {
  _infoBarContainer->SuspendInfobars();
}


- (void)updateInfobarContainer {
  [self infoBarContainerStateDidChangeAnimated:NO];
}

#pragma mark - InfobarContainerStateDelegate

- (void)infoBarContainerStateDidChangeAnimated:(BOOL)animated {
  InfoBarContainerView* infoBarContainerView = _infoBarContainer->view();
  DCHECK(infoBarContainerView);
  CGRect containerFrame = infoBarContainerView.frame;
  CGFloat height = [infoBarContainerView topmostVisibleInfoBarHeight];
  containerFrame.origin.y =
      CGRectGetMaxY([self.positioner parentView].frame) - height;
  containerFrame.size.height = height;

  BOOL isViewVisible = [self.positioner isParentViewVisible];
  ProceduralBlock frameUpdates = ^{
    [infoBarContainerView setFrame:containerFrame];
  };
  void (^completion)(BOOL) = ^(BOOL finished) {
    if (!isViewVisible)
      return;
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    infoBarContainerView);
  };
  if (animated) {
    [UIView animateWithDuration:0.1
                     animations:frameUpdates
                     completion:completion];
  } else {
    frameUpdates();
    completion(YES);
  }
}

#pragma mark - TabModelObserver methods

- (void)tabModel:(TabModel*)model
    didChangeActiveTab:(Tab*)newTab
           previousTab:(Tab*)previousTab
               atIndex:(NSUInteger)index {
  DCHECK(newTab.webState);
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(newTab.webState);
  _infoBarContainer->ChangeInfoBarManager(infoBarManager);
}

- (void)tabModel:(TabModel*)model
    newTabWillOpen:(Tab*)tab
      inBackground:(BOOL)background {
  // When adding new tabs, check what kind of reminder infobar should
  // be added to the new tab. Try to add only one of them.
  // This check is done when a new tab is added either through the Tools Menu
  // "New Tab", through a long press on the Tab Switcher button "New Tab", and
  // through creating a New Tab from the Tab Switcher. This method is called
  // after a new tab has added and finished initial navigation. If this is added
  // earlier, the initial navigation may end up clearing the infobar(s) that are
  // just added.
  web::WebState* webState = tab.webState;
  DCHECK(webState);

  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(webState);
  [[UpgradeCenter sharedInstance] addInfoBarToManager:infoBarManager
                                             forTabId:[tab tabId]];

  if (!ReSignInInfoBarDelegate::Create(self.browserState, tab,
                                       self /* id<SigninPresenter> */)) {
    DisplaySyncErrors(self.browserState, tab,
                      self.syncPresenter /* id<SyncPresenter> */);
  }
}

- (void)tabModel:(TabModel*)model
    didReplaceTab:(Tab*)oldTab
          withTab:(Tab*)newTab
          atIndex:(NSUInteger)index {
  infobars::InfoBarManager* infoBarManager = nullptr;
  if (newTab) {
    DCHECK(newTab.webState);
    infoBarManager = InfoBarManagerImpl::FromWebState(newTab.webState);
  }
  _infoBarContainer->ChangeInfoBarManager(infoBarManager);
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  [self.dispatcher showSignin:command
           baseViewController:self.baseViewController];
}

@end
