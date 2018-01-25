// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive/primary_toolbar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/google/core/browser/google_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_controller_impl.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_positioner.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/adaptive/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/adaptive/primary_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/referrer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrimaryToolbarCoordinator ()<OmniboxPopupPositioner> {
  std::unique_ptr<LocationBarControllerImpl> _locationBar;
  // Observer that updates |toolbarViewController| for fullscreen events.
  std::unique_ptr<FullscreenControllerObserver> _fullscreenObserver;
}

// Redefined as PrimaryToolbarViewController.
@property(nonatomic, strong) PrimaryToolbarViewController* viewController;
// The coordinator for the location bar in the toolbar.
@property(nonatomic, strong) LocationBarCoordinator* locationBarCoordinator;
// Coordinator for the omnibox popup.
@property(nonatomic, strong) OmniboxPopupCoordinator* omniboxPopupCoordinator;

@end

@implementation PrimaryToolbarCoordinator

@dynamic viewController;
@synthesize locationBarCoordinator = _locationBarCoordinator;
@synthesize omniboxPopupCoordinator = _omniboxPopupCoordinator;
@synthesize delegate = _delegate;
@synthesize URLLoader = _URLLoader;

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewController = [[PrimaryToolbarViewController alloc] init];
  self.viewController.buttonFactory = [self buttonFactoryWithType:PRIMARY];

  [self setUpLocationBar];
  self.viewController.locationBarView =
      self.locationBarCoordinator.locationBarView;

  [super start];

  _fullscreenObserver =
      std::make_unique<FullscreenUIUpdater>(self.viewController);
  FullscreenControllerFactory::GetInstance()
      ->GetForBrowserState(self.browserState)
      ->AddObserver(_fullscreenObserver.get());
}

#pragma mark - PrimaryToolbarCoordinator

- (id<VoiceSearchControllerDelegate>)voiceSearchDelegate {
  // TODO(crbug.com/799446): This code should be moved to the location bar.
  return nil;
}

- (id<QRScannerResultLoading>)QRScannerResultLoader {
  // TODO(crbug.com/799446): This code should be moved to the location bar.
  return nil;
}

- (id<TabHistoryUIUpdater>)tabHistoryUIUpdater {
  return self.viewController;
}

- (id<ActivityServicePositioner>)activityServicePositioner {
  return self.viewController;
}

- (id<OmniboxFocuser>)omniboxFocuser {
  return self.locationBarCoordinator;
}

- (void)showPrerenderingAnimation {
  [self.viewController showPrerenderingAnimation];
}

- (BOOL)isOmniboxFirstResponder {
  return
      [self.locationBarCoordinator.locationBarView.textField isFirstResponder];
}

- (BOOL)showingOmniboxPopup {
  OmniboxViewIOS* omniboxViewIOS =
      static_cast<OmniboxViewIOS*>(_locationBar.get()->GetLocationEntry());
  return omniboxViewIOS->IsPopupOpen();
}

- (void)transitionToLocationBarFocusedState:(BOOL)focused {
  // TODO(crbug.com/801082): implement this.
}

#pragma mark - FakeboxFocuser

- (void)focusFakebox {
  // TODO(crbug.com/803372): Implement that.
}

- (void)onFakeboxBlur {
  // TODO(crbug.com/803372): Implement that.
}

- (void)onFakeboxAnimationComplete {
  // TODO(crbug.com/803372): Implement that.
}

// TODO(crbug.com/786940): This protocol should move to the ViewController
// owning the Toolbar. This can wait until the omnibox and toolbar refactoring
// is more advanced.
#pragma mark OmniboxPopupPositioner methods.

- (UIView*)popupAnchorView {
  return self.viewController.view;
}

- (UIView*)popupParentView {
  return self.viewController.view.superview;
}

#pragma mark - SideSwipeToolbarInteracting

- (UIView*)toolbarView {
  return self.viewController.view;
}

- (BOOL)canBeginToolbarSwipe {
  return ![self isOmniboxFirstResponder] && ![self showingOmniboxPopup];
}

- (UIImage*)toolbarSideSwipeSnapshotForTab:(Tab*)tab {
  // TODO(crbug.com/803371): Implement that.
  return nil;
}

#pragma mark - Private

// Sets the location bar up.
- (void)setUpLocationBar {
  self.locationBarCoordinator = [[LocationBarCoordinator alloc] init];
  self.locationBarCoordinator.browserState = self.browserState;
  self.locationBarCoordinator.dispatcher = self.dispatcher;
  self.locationBarCoordinator.URLLoader = self.URLLoader;
  self.locationBarCoordinator.delegate = self.delegate;
  self.locationBarCoordinator.webStateList = self.webStateList;
  [self.locationBarCoordinator start];

  // TODO(crbug.com/785253): Move this to the LocationBarCoordinator once it is
  // created.
  _locationBar = std::make_unique<LocationBarControllerImpl>(
      self.locationBarCoordinator.locationBarView, self.browserState,
      self.locationBarCoordinator, self.dispatcher);
  self.locationBarCoordinator.locationBarController = _locationBar.get();
  _locationBar->SetURLLoader(self.locationBarCoordinator);
  self.omniboxPopupCoordinator = _locationBar->CreatePopupCoordinator(self);
  [self.omniboxPopupCoordinator start];
  // End of TODO(crbug.com/785253):.
}

@end
