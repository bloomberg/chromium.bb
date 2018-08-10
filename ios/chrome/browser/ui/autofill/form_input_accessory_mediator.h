// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/autofill/form_input_accessory_view_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

@protocol FormInputAccessoryConsumer;
@protocol FormInputAccessoryViewProvider;
@class JsSuggestionManager;
class WebStateList;
namespace web {
class WebState;
}

// This class contains all the logic to get and provide keyboard input accessory
// views to its consumer. As well as telling the consumer when the default
// accessory view shoeuld be restored to the system default.
@interface FormInputAccessoryMediator : NSObject

// Returns a mediator observing the passed `WebStateList` and associated with
// the passed consumer. `webSateList` can be nullptr and `consumer` can be nil.
- (instancetype)initWithConsumer:(id<FormInputAccessoryConsumer>)consumer
                    webStateList:(WebStateList*)webStateList;

// Unavailable, use initWithConsumer:webStateList: instead.
- (instancetype)init NS_UNAVAILABLE;

@end

// Methods to allow injection in tests.
@interface FormInputAccessoryMediator (Tests)

// The WebState this instance is observing. Can be null.
- (void)injectWebState:(web::WebState*)webState;

// The JS manager for interacting with the underlying form.
- (void)injectSuggestionManager:(JsSuggestionManager*)JSSuggestionManager;

// The objects that can provide a custom input accessory view while filling
// forms.
- (void)injectProviders:(NSArray<id<FormInputAccessoryViewProvider>>*)providers;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_MEDIATOR_H_
