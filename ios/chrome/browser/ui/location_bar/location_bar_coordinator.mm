// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/google/core/browser/google_util.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_consumer.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_mediator.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_url_loader.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_controller.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_controller_impl.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_views.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ios/web/public/referrer.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LocationBarCoordinator ()<LocationBarConsumer> {
  std::unique_ptr<LocationBarControllerImpl> _locationBarController;
}
// Object taking care of adding the accessory views to the keyboard.
@property(nonatomic, strong)
    ToolbarAssistiveKeyboardDelegateImpl* keyboardDelegate;
// Coordinator for the omnibox popup.
@property(nonatomic, strong) OmniboxPopupCoordinator* omniboxPopupCoordinator;
@property(nonatomic, strong) LocationBarMediator* mediator;
@end

@implementation LocationBarCoordinator
@synthesize locationBarView = _locationBarView;
@synthesize mediator = _mediator;
@synthesize keyboardDelegate = _keyboardDelegate;
@synthesize browserState = _browserState;
@synthesize dispatcher = dispatcher;
@synthesize URLLoader = _URLLoader;
@synthesize delegate = _delegate;
@synthesize webStateList = _webStateList;
@synthesize omniboxPopupCoordinator = _omniboxPopupCoordinator;
@synthesize popupPositioner = _popupPositioner;

#pragma mark - public

- (void)start {
  BOOL isIncognito = self.browserState->IsOffTheRecord();

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
  SetA11yLabelAndUiAutomationName(self.locationBarView.textField,
                                  IDS_ACCNAME_LOCATION, @"Address");
  self.locationBarView.incognito = isIncognito;
  self.locationBarView.textField.incognito = isIncognito;
  if (isIncognito) {
    [_locationBarView.textField
        setSelectedTextBackgroundColor:[UIColor colorWithWhite:1 alpha:0.1]];
    [_locationBarView.textField
        setPlaceholderTextColor:[UIColor colorWithWhite:1 alpha:0.5]];
  } else if (!IsIPadIdiom()) {
    // Set placeholder text color to match fakebox placeholder text color when
    // on iPhone.
    UIColor* placeholderTextColor =
        [UIColor colorWithWhite:kiPhoneOmniboxPlaceholderColorBrightness
                          alpha:1.0];
    [_locationBarView.textField setPlaceholderTextColor:placeholderTextColor];
  }

  self.keyboardDelegate = [[ToolbarAssistiveKeyboardDelegateImpl alloc] init];
  self.keyboardDelegate.dispatcher = self.dispatcher;
  self.keyboardDelegate.omniboxTextField = self.locationBarView.textField;
  ConfigureAssistiveKeyboardViews(self.locationBarView.textField, kDotComTLD,
                                  self.keyboardDelegate);

  _locationBarController = std::make_unique<LocationBarControllerImpl>(
      self.locationBarView, self.browserState, self, self.dispatcher);
  _locationBarController->SetURLLoader(self);
  self.omniboxPopupCoordinator =
      _locationBarController->CreatePopupCoordinator(self.popupPositioner);
  [self.omniboxPopupCoordinator start];
  self.mediator = [[LocationBarMediator alloc] init];
  self.mediator.webStateList = self.webStateList;
  self.mediator.consumer = self;
}

- (void)stop {
  // The popup has to be destroyed before the location bar.
  [self.omniboxPopupCoordinator stop];
  _locationBarController.reset();

  self.locationBarView = nil;
  [self.mediator disconnect];
  self.mediator = nil;
}

- (BOOL)omniboxPopupHasAutocompleteResults {
  return self.omniboxPopupCoordinator.hasResults;
}

#pragma mark - LocationBarConsumer

- (void)updateOmniboxState {
  _locationBarController->SetShouldShowHintText(
      [self.delegate toolbarModelIOS]->ShouldDisplayHintText());
  _locationBarController->OnToolbarUpdated();
}

- (BOOL)showingOmniboxPopup {
  OmniboxViewIOS* omniboxViewIOS = static_cast<OmniboxViewIOS*>(
      _locationBarController.get()->GetLocationEntry());
  return omniboxViewIOS->IsPopupOpen();
}

- (void)focusOmniboxFromFakebox {
  OmniboxEditModel* model = _locationBarController->GetLocationEntry()->model();
  // Setting the caret visibility to false causes OmniboxEditModel to indicate
  // that omnibox interaction was initiated from the fakebox. Note that
  // SetCaretVisibility is a no-op unless OnSetFocus is called first.  Only
  // set fakebox on iPad, where there is a distinction between the omnibox
  // and the fakebox on the NTP.
  model->OnSetFocus(false);
  model->SetCaretVisibility(false);
}

#pragma mark - LocationBarURLLoader

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

// This will be OmniboxFocuser implementation, but it's not yet ready. Some
// methods are already necessary though.
#pragma mark - OmniboxFocuser

- (void)focusOmnibox {
  [self.locationBarView.textField becomeFirstResponder];
}

- (void)cancelOmniboxEdit {
  _locationBarController->HideKeyboardAndEndEditing();
  [self updateOmniboxState];
}

#pragma mark - LocationBarDelegate

- (void)locationBarHasBecomeFirstResponder {
  [self.delegate locationBarDidBecomeFirstResponder];
}

- (void)locationBarHasResignedFirstResponder {
  [self.delegate locationBarDidResignFirstResponder];
}

- (void)locationBarBeganEdit {
  [self.delegate locationBarBeganEdit];
}

- (web::WebState*)webState {
  return self.webStateList->GetActiveWebState();
}

- (ToolbarModel*)toolbarModel {
  ToolbarModelIOS* toolbarModelIOS = [self.delegate toolbarModelIOS];
  return toolbarModelIOS ? toolbarModelIOS->GetToolbarModel() : nullptr;
}

@end
