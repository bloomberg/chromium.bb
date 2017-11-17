// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/google/core/browser/google_util.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_controller.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_controller_impl.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_mediator.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_style.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarCoordinator ()<LocationBarDelegate> {
  std::unique_ptr<LocationBarControllerImpl> _locationBar;
}

// The View Controller managed by this coordinator.
@property(nonatomic, strong) ToolbarViewController* viewController;
// The mediator owned by this coordinator.
@property(nonatomic, strong) ToolbarMediator* mediator;
// LocationBarView containing the omnibox. At some point, this property and the
// |_locationBar| should become a LocationBarCoordinator.
@property(nonatomic, strong) LocationBarView* locationBarView;

@end

@implementation ToolbarCoordinator
@synthesize delegate = _delegate;
@synthesize browserState = _browserState;
@synthesize dispatcher = _dispatcher;
@synthesize locationBarView = _locationBarView;
@synthesize mediator = _mediator;
@synthesize URLLoader = _URLLoader;
@synthesize viewController = _viewController;
@synthesize webStateList = _webStateList;

- (instancetype)init {
  if ((self = [super init])) {
    _mediator = [[ToolbarMediator alloc] init];
  }
  return self;
}

#pragma mark - BrowserCoordinator

- (void)start {
  BOOL isIncognito = self.browserState->IsOffTheRecord();
  // TODO(crbug.com/785253): Move this to the LocationBarCoordinator once it is
  // created.
  UIColor* textColor =
      isIncognito
          ? [UIColor whiteColor]
          : [UIColor colorWithWhite:0 alpha:[MDCTypography body1FontOpacity]];
  UIColor* tintColor = isIncognito ? textColor : nil;
  self.locationBarView =
      [[LocationBarView alloc] initWithFrame:CGRectZero
                                        font:[MDCTypography subheadFont]
                                   textColor:textColor
                                   tintColor:tintColor];
  _locationBar = base::MakeUnique<LocationBarControllerImpl>(
      self.locationBarView, self.browserState, self, self.dispatcher);
  // End of TODO(crbug.com/785253):.

  ToolbarStyle style = isIncognito ? INCOGNITO : NORMAL;
  ToolbarButtonFactory* factory =
      [[ToolbarButtonFactory alloc] initWithStyle:style];

  self.viewController =
      [[ToolbarViewController alloc] initWithDispatcher:self.dispatcher
                                          buttonFactory:factory];

  self.mediator.consumer = self.viewController;
  self.mediator.webStateList = self.webStateList;
}

- (void)stop {
  [self.mediator disconnect];
  _locationBar.reset();
  self.locationBarView = nil;
}

#pragma mark - Public

- (void)updateToolbarState {
  // TODO(crbug.com/784911): This function should probably triggers something in
  // the mediator. Investigate how to handle it.
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
  if (@available(iOS 10, *)) {
    [self.viewController expandOmniboxAnimated:YES];
  }
}

- (void)locationBarHasResignedFirstResponder {
  [self.delegate locationBarDidResignFirstResponder];
  if (@available(iOS 10, *)) {
    [self.viewController contractOmnibox];
  }
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

#pragma mark - OmniboxFocuser

- (void)focusOmnibox {
  if (!self.viewController.view.hidden)
    [_locationBarView.textField becomeFirstResponder];
}

- (void)cancelOmniboxEdit {
  _locationBar->HideKeyboardAndEndEditing();
  [self updateToolbarState];
}

- (void)focusFakebox {
  if (IsIPadIdiom()) {
    OmniboxEditModel* model = _locationBar->GetLocationEntry()->model();
    // Setting the caret visibility to false causes OmniboxEditModel to indicate
    // that omnibox interaction was initiated from the fakebox. Note that
    // SetCaretVisibility is a no-op unless OnSetFocus is called first.  Only
    // set fakebox on iPad, where there is a distinction between the omnibox
    // and the fakebox on the NTP.  On iPhone there is no visible omnibox, so
    // there's no need to indicate interaction was initiated from the fakebox.
    model->OnSetFocus(false);
    model->SetCaretVisibility(false);
  } else {
    [self.viewController expandOmniboxAnimated:NO];
  }

  [self focusOmnibox];
}

- (void)onFakeboxBlur {
  DCHECK(!IsIPadIdiom());
  // Hide the toolbar if the NTP is currently displayed.
  web::WebState* webState = [self getWebState];
  if (webState && (webState->GetVisibleURL() == GURL(kChromeUINewTabURL))) {
    self.viewController.view.hidden = YES;
  }
}

- (void)onFakeboxAnimationComplete {
  DCHECK(!IsIPadIdiom());
  self.viewController.view.hidden = NO;
}

@end
