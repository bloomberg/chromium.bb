// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"

#include "base/mac/foundation_util.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_handler.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManualFillInjectionHandler ()
// The WebStateList with the relevant active web state for the injection.
@property(nonatomic, assign) WebStateList* webStateList;
// Convenience getter for the current injection reciever.
@property(nonatomic, readonly) CRWJSInjectionReceiver* injectionReceiver;
// Convenience getter for the current suggestion manager.
@property(nonatomic, readonly) JsSuggestionManager* suggestionManager;
@end

@implementation ManualFillInjectionHandler

@synthesize webStateList = _webStateList;

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
  }
  return self;
}

#pragma mark - ManualFillViewDelegate

- (void)userDidPickContent:(NSString*)content isSecure:(BOOL)isSecure {
  if (isSecure) {
    // TODO:(https://crbug.com/878388) implement secure manual fill for
    // passwords.
    return;
  }
  [self fillLastSelectedFieldWithString:content];
}

#pragma mark - Getters

- (CRWJSInjectionReceiver*)injectionReceiver {
  web::WebState* webState = self.webStateList->GetActiveWebState();
  if (webState) {
    return webState->GetJSInjectionReceiver();
  }
  return nil;
}

- (JsSuggestionManager*)suggestionManager {
  return base::mac::ObjCCastStrict<JsSuggestionManager>(
      [self.injectionReceiver instanceOfClass:[JsSuggestionManager class]]);
}

#pragma mark - Document Interaction

// Injects the passed string to the active field and jumps to the next field.
- (void)fillLastSelectedFieldWithString:(NSString*)string {
  // TODO:(https://crbug.com/878388) validation / escaping of string.
  NSString* javaScriptQuery =
      [NSString stringWithFormat:
                    @"__gCrWeb.fill.setInputElementValue(\"%@\", "
                    @"document.activeElement);",
                    string];
  [self.injectionReceiver executeJavaScript:javaScriptQuery
                          completionHandler:^(id, NSError*) {
                            [self jumpToNextField];
                          }];
}

// Attempts to jump to the next field in the current form.
- (void)jumpToNextField {
  FormInputAccessoryViewHandler* handler =
      [[FormInputAccessoryViewHandler alloc] init];
  handler.JSSuggestionManager = self.suggestionManager;
  [handler selectNextElementWithoutButtonPress];
}

@end
