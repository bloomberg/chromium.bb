// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"

#include "base/memory/ptr_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_views.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

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

@end
