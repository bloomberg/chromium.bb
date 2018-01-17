// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive/primary_toolbar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/google/core/browser/google_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_controller_impl.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_positioner.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/adaptive/primary_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/referrer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrimaryToolbarCoordinator ()<LocationBarDelegate,
                                        OmniboxPopupPositioner> {
  std::unique_ptr<LocationBarControllerImpl> _locationBar;
}

// Whether this coordinator has been started.
@property(nonatomic, assign) BOOL started;
// The Toolbar view controller owned by this coordinator.
@property(nonatomic, strong)
    PrimaryToolbarViewController* toolbarViewController;
// The coordinator for the location bar in the toolbar.
@property(nonatomic, strong) LocationBarCoordinator* locationBarCoordinator;
// Coordinator for the omnibox popup.
@property(nonatomic, strong) OmniboxPopupCoordinator* omniboxPopupCoordinator;

@end

@implementation PrimaryToolbarCoordinator
@synthesize delegate = _delegate;
@synthesize dispatcher = _dispatcher;
@synthesize locationBarCoordinator = _locationBarCoordinator;
@synthesize omniboxPopupCoordinator = _omniboxPopupCoordinator;
@synthesize started = _started;
@synthesize toolbarViewController = _toolbarViewController;
@synthesize URLLoader = _URLLoader;
@synthesize webStateList = _webStateList;

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  return [super initWithBaseViewController:nil browserState:browserState];
}

- (void)start {
  if (self.started)
    return;

  self.started = YES;
  BOOL isIncognito = self.browserState->IsOffTheRecord();

  self.locationBarCoordinator = [[LocationBarCoordinator alloc] init];
  self.locationBarCoordinator.browserState = self.browserState;
  self.locationBarCoordinator.dispatcher = self.dispatcher;
  [self.locationBarCoordinator start];

  // TODO(crbug.com/785253): Move this to the LocationBarCoordinator once it is
  // created.
  _locationBar = std::make_unique<LocationBarControllerImpl>(
      self.locationBarCoordinator.locationBarView, self.browserState, self,
      self.dispatcher);
  self.omniboxPopupCoordinator = _locationBar->CreatePopupCoordinator(self);
  [self.omniboxPopupCoordinator start];
  // End of TODO(crbug.com/785253):.

  ToolbarStyle style = isIncognito ? INCOGNITO : NORMAL;
  ToolbarButtonFactory* buttonFactory =
      [[ToolbarButtonFactory alloc] initWithStyle:style];
  buttonFactory.dispatcher = self.dispatcher;
  buttonFactory.visibilityConfiguration =
      [[ToolbarButtonVisibilityConfiguration alloc] initWithType:PRIMARY];

  self.toolbarViewController = [[PrimaryToolbarViewController alloc]
      initWithButtonFactory:buttonFactory];
  self.toolbarViewController.dispatcher = self.dispatcher;
  self.toolbarViewController.locationBarView =
      self.locationBarCoordinator.locationBarView;
}

- (void)stop {
  self.started = NO;
  self.toolbarViewController = nil;
  [self.omniboxPopupCoordinator stop];
  [self.locationBarCoordinator stop];
}

#pragma mark - Property Accessors

- (UIViewController*)viewController {
  return self.toolbarViewController;
}

#pragma mark - PrimaryToolbarCoordinator

- (id<VoiceSearchControllerDelegate>)voiceSearchDelegate {
  // TODO(crbug.com/799438): Implement that.
  return nil;
}

- (id<QRScannerResultLoading>)QRScannerResultLoader {
  // TODO(crbug.com/799438): Implement that.
  return nil;
}

- (id<TabHistoryUIUpdater>)tabHistoryUIUpdater {
  // TODO(crbug.com/799438): Implement that.
  return nil;
}

- (id<ActivityServicePositioner>)activityServicePositioner {
  // TODO(crbug.com/799438): Implement that.
  return nil;
}

- (void)showPrerenderingAnimation {
  // TODO(crbug.com/799438): Implement that.
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

#pragma mark - ToolbarCoordinating

- (void)updateToolbarState {
}

- (void)setToolbarBackgroundAlpha:(CGFloat)alpha {
}

#pragma mark - ToolbarCommands

- (void)contractToolbar {
  // TODO(crbug.com/801082): Implement that.
}

- (void)triggerToolsMenuButtonAnimation {
  // TODO(crbug.com/801083): Implement that.
}

- (void)navigateToMemexTabSwitcher {
  // TODO(crbug.com/799601): Delete this once it is not needed.
}

#pragma mark - OmniboxFocuser

- (void)focusOmnibox {
  // TODO(crbug.com/799438): Implement that.
}

- (void)cancelOmniboxEdit {
  // TODO(crbug.com/799438): Implement that.
}

- (void)focusFakebox {
  // TODO(crbug.com/799438): Implement that.
}

- (void)onFakeboxBlur {
  // TODO(crbug.com/799438): Implement that.
}

- (void)onFakeboxAnimationComplete {
  // TODO(crbug.com/799438): Implement that.
}

#pragma mark - SideSwipeToolbarInteracting

- (UIView*)toolbarView {
  return self.viewController.view;
}

- (BOOL)canBeginToolbarSwipe {
  return ![self isOmniboxFirstResponder] && ![self showingOmniboxPopup];
}

- (UIImage*)toolbarSideSwipeSnapshotForTab:(Tab*)tab {
  // TODO(crbug.com/799438): Implement that.
  return nil;
}

// TODO(crbug.com/786940): This protocol should move to the ViewController
// owning the Toolbar. This can wait until the omnibox and toolbar refactoring
// is more advanced.
#pragma mark OmniboxPopupPositioner methods.

- (UIView*)popupAnchorView {
  return self.toolbarViewController.view;
}

#pragma mark - LocationBarDelegate

- (void)loadGURLFromLocationBar:(const GURL&)url
                     transition:(ui::PageTransition)transition {
  if (url.SchemeIs(url::kJavaScriptScheme)) {
    // Evaluate the URL as JavaScript if its scheme is JavaScript.
    NSString* jsToEval = [base::SysUTF8ToNSString(url.GetContent())
        stringByRemovingPercentEncoding];
    [self.URLLoader loadJavaScriptFromLocationBar:jsToEval];
  } else {
    // When opening a URL, force the omnibox to resign first responder.  This
    // will also close the popup.

    // TODO(crbug.com/785244): Is it ok to call |cancelOmniboxEdit| after
    // |loadURL|?  It doesn't seem to be causing major problems.  If we call
    // cancel before load, then any prerendered pages get destroyed before the
    // call to load.
    [self.URLLoader loadURL:url
                   referrer:web::Referrer()
                 transition:transition
          rendererInitiated:NO];

    if (google_util::IsGoogleSearchUrl(url)) {
      UMA_HISTOGRAM_ENUMERATION(
          kOmniboxQueryLocationAuthorizationStatusHistogram,
          [CLLocationManager authorizationStatus],
          kLocationAuthorizationStatusCount);
    }
  }
  [self cancelOmniboxEdit];
}

- (void)locationBarHasBecomeFirstResponder {
  [self.delegate locationBarDidBecomeFirstResponder];
}

- (void)locationBarHasResignedFirstResponder {
  [self.delegate locationBarDidResignFirstResponder];
}

- (void)locationBarBeganEdit {
  [self.delegate locationBarBeganEdit];
}

- (web::WebState*)getWebState {
  return self.webStateList->GetActiveWebState();
}

- (ToolbarModel*)toolbarModel {
  ToolbarModelIOS* toolbarModelIOS = [self.delegate toolbarModelIOS];
  return toolbarModelIOS ? toolbarModelIOS->GetToolbarModel() : nullptr;
}

@end
