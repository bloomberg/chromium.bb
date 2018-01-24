// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/google/core/browser/google_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_url_loader.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_controller_impl.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_views.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ios/web/public/referrer.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LocationBarCoordinator ()
// Object taking care of adding the accessory views to the keyboard.
@property(nonatomic, strong)
    ToolbarAssistiveKeyboardDelegateImpl* keyboardDelegate;
@end

@implementation LocationBarCoordinator
@synthesize locationBarView = _locationBarView;
@synthesize keyboardDelegate = _keyboardDelegate;
@synthesize browserState = _browserState;
@synthesize dispatcher = dispatcher;
@synthesize URLLoader = _URLLoader;
@synthesize locationBarController = _locationBarController;
@synthesize delegate = _delegate;

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
}

- (void)stop {
  self.locationBarView = nil;
}

- (void)updateOmniboxState {
  _locationBarController->SetShouldShowHintText(
      [self.delegate toolbarModelIOS]->ShouldDisplayHintText());
  _locationBarController->OnToolbarUpdated();
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
@end
