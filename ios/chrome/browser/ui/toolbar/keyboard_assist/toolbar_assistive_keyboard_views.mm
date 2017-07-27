// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_views.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_keyboard_accessory_view.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

UIButton* ButtonWitIcon(NSString* iconName) {
  const CGFloat kButtonShadowOpacity = 0.35;
  const CGFloat kButtonShadowRadius = 1.0;
  const CGFloat kButtonShadowVerticalOffset = 1.0;

  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  UIImage* icon = [UIImage imageNamed:iconName];
  [button setImage:icon forState:UIControlStateNormal];
  button.layer.shadowColor = [UIColor blackColor].CGColor;
  button.layer.shadowOffset = CGSizeMake(0, kButtonShadowVerticalOffset);
  button.layer.shadowOpacity = kButtonShadowOpacity;
  button.layer.shadowRadius = kButtonShadowRadius;
  return button;
}

}  // namespace

NSArray<UIButton*>* ToolbarAssistiveKeyboardLeadingButtons(
    id<ToolbarAssistiveKeyboardDelegate> delegate) {
  UIButton* voiceSearchButton =
      ButtonWitIcon(@"keyboard_accessory_voice_search");
  [voiceSearchButton addTarget:delegate
                        action:@selector(keyboardAccessoryVoiceSearchTouchDown:)
              forControlEvents:UIControlEventTouchDown];
  SetA11yLabelAndUiAutomationName(voiceSearchButton,
                                  IDS_IOS_KEYBOARD_ACCESSORY_VIEW_VOICE_SEARCH,
                                  @"Voice Search");
  [voiceSearchButton
             addTarget:delegate
                action:@selector(keyboardAccessoryVoiceSearchTouchUpInside:)
      forControlEvents:UIControlEventTouchUpInside];

  UIButton* cameraButton = ButtonWitIcon(@"keyboard_accessory_qr_scanner");
  [cameraButton addTarget:delegate
                   action:@selector(keyboardAccessoryCameraSearchTouchUp)
         forControlEvents:UIControlEventTouchUpInside];
  SetA11yLabelAndUiAutomationName(
      cameraButton, IDS_IOS_KEYBOARD_ACCESSORY_VIEW_QR_CODE_SEARCH,
      @"QR code Search");

  return @[ voiceSearchButton, cameraButton ];
}

void ConfigureAssistiveKeyboardViews(
    UITextField* textField,
    NSString* dotComTLD,
    id<ToolbarAssistiveKeyboardDelegate> delegate) {
  DCHECK(dotComTLD);
  textField.inputAssistantItem.leadingBarButtonGroups = @[];
  textField.inputAssistantItem.trailingBarButtonGroups = @[];

  NSArray<NSString*>* buttonTitles = @[ @":", @"-", @"/", dotComTLD ];
  UIView* keyboardAccessoryView =
      [[ToolbarKeyboardAccessoryView alloc] initWithButtons:buttonTitles
                                                   delegate:delegate];
  [keyboardAccessoryView setAutoresizingMask:UIViewAutoresizingFlexibleWidth];

  [textField setInputAccessoryView:keyboardAccessoryView];
}
