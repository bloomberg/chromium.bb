// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_delegate.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/external_search_commands.h"
#import "ios/chrome/browser/ui/commands/start_voice_search_command.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/voice/voice_search_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ToolbarAssistiveKeyboardDelegateImpl

@synthesize dispatcher = _dispatcher;
@synthesize omniboxTextField = _omniboxTextField;

#pragma mark - Public

- (void)keyboardAccessoryVoiceSearchTouchDown:(UIView*)view {
  if (ios::GetChromeBrowserProvider()
          ->GetVoiceSearchProvider()
          ->IsVoiceSearchEnabled()) {
    [self.dispatcher preloadVoiceSearch];
  }
}

- (void)keyboardAccessoryVoiceSearchTouchUpInside:(UIView*)view {
  if (ios::GetChromeBrowserProvider()
          ->GetVoiceSearchProvider()
          ->IsVoiceSearchEnabled()) {
    base::RecordAction(base::UserMetricsAction("MobileCustomRowVoiceSearch"));
    StartVoiceSearchCommand* command =
        [[StartVoiceSearchCommand alloc] initWithOriginView:view];
    [self.dispatcher startVoiceSearch:command];
  }
}

- (void)keyboardAccessoryCameraSearchTouchUp {
  base::RecordAction(base::UserMetricsAction("MobileCustomRowCameraSearch"));
  [self.dispatcher showQRScanner];
}

- (void)keyboardAccessoryExternalSearchTouchUp {
  [self.dispatcher launchExternalSearch];
}

- (void)keyPressed:(NSString*)title {
  NSString* text = [self updateTextForDotCom:title];
  [self.omniboxTextField insertTextWhileEditing:text];
}

#pragma mark - Private

// Insert 'com' without the period if cursor is directly after a period.
- (NSString*)updateTextForDotCom:(NSString*)text {
  if ([text isEqualToString:kDotComTLD]) {
    UITextRange* textRange = [self.omniboxTextField selectedTextRange];
    NSInteger pos = [self.omniboxTextField
        offsetFromPosition:[self.omniboxTextField beginningOfDocument]
                toPosition:textRange.start];
    if (pos > 0 &&
        [[self.omniboxTextField text] characterAtIndex:pos - 1] == '.')
      return [kDotComTLD substringFromIndex:1];
  }
  return text;
}

@end
