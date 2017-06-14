// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <string>

#import "components/autofill/ios/browser/js_suggestion_manager.h"
#import "ios/chrome/browser/autofill/form_suggestion_view.h"
#import "ios/chrome/browser/autofill/form_suggestion_view_client.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

namespace web {
class WebState;
}

@protocol CRWWebViewProxy;
@protocol FormInputAccessoryViewProvider;

// Handles form focus events and presents input suggestions.
@interface FormSuggestionController
    : NSObject<CRWWebStateObserver, FormSuggestionViewClient>

// Initializes a new FormSuggestionController with the specified WebState and a
// list of FormSuggestionProviders.
// When suggestions are required for an input field, the |providers| will be
// asked (in order) if they can handle the field; the first provider to return
// YES from [FormSuggestionProvider canProviderSuggestionsForForm:field:] will
// be expected to provide those suggestions using [FormSuggestionProvider
// retrieveSuggestionsForForm:field:withCompletion:].
- (instancetype)initWithWebState:(web::WebState*)webState
                       providers:(NSArray*)providers;

// Finds a FormSuggestionProvider that can supply suggestions for the specified
// form, requests them, and updates the view accordingly.
- (void)retrieveSuggestionsForFormNamed:(const std::string&)formName
                              fieldName:(const std::string&)fieldName
                                   type:(const std::string&)type
                               webState:(web::WebState*)webState;

// Instructs the controller to detach itself from the WebState.
- (void)detachFromWebState;

// Provides an input accessory view for form suggestions.
@property(nonatomic, readonly) id<FormInputAccessoryViewProvider>
    accessoryViewProvider;

@end

@interface FormSuggestionController (ForTesting)

// Initializes a new controller in the same way as the public initializer, but
// supports specifying a JsSuggestionManager for testing.
- (instancetype)initWithWebState:(web::WebState*)webState
                       providers:(NSArray*)providers
             JsSuggestionManager:(JsSuggestionManager*)jsSuggestionManager;

// Overrides the web view proxy.
- (void)setWebViewProxy:(id<CRWWebViewProxy>)webViewProxy;

// Invoked when an attempt to retrieve suggestions yields no results.
- (void)onNoSuggestionsAvailable;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_CONTROLLER_H_
