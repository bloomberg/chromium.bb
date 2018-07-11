// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/form_input_accessory_view_delegate.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

@protocol CRWWebViewProxy;
@protocol FormInputAccessoryViewProvider;

namespace autofill {
extern NSString* const kFormSuggestionAssistButtonPreviousElement;
extern NSString* const kFormSuggestionAssistButtonNextElement;
extern NSString* const kFormSuggestionAssistButtonDone;
extern CGFloat const kInputAccessoryHeight;
}  // namespace autofill

// Creates and manages a custom input accessory view while the user is
// interacting with a form. Also handles hiding and showing the default
// accessory view elements.
@interface FormInputAccessoryViewController
    : NSObject<CRWWebStateObserver, FormInputAccessoryViewDelegate>

// The current web view proxy.
// TODO(crbug.com/727716): This property should not be a part of the public
// interface, it is used in tests as a backdoor.
@property(nonatomic, readonly) id<CRWWebViewProxy> webViewProxy;

// The current web state that this view controller is observing.
@property(nonatomic) web::WebState* webState;

// Initializes a new controller with the specified |providers| of input
// accessory views.
- (instancetype)initWithWebState:(web::WebState*)webState
                       providers:(NSArray<id<FormInputAccessoryViewProvider>>*)
                                     providers;

// Instructs the controller to detach itself from the WebState.
- (void)detachFromWebState;

// Hides the default input accessory view and replaces it with one that shows
// |customView| and form navigation controls.
- (void)showCustomInputAccessoryView:(UIView*)customView;

// Restores the default input accessory view, removing (if necessary) any
// previously-added custom view.
- (void)restoreDefaultInputAccessoryView;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_H_
