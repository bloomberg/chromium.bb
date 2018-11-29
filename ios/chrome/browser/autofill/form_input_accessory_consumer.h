// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_CONSUMER_H_

#import <Foundation/Foundation.h>

@class FormSuggestion;
@protocol FormInputAccessoryViewDelegate;
@protocol FormSuggestionClient;

@protocol FormInputAccessoryConsumer<NSObject>

// Delegate used for form navigation.
@property(nonatomic, weak) id<FormInputAccessoryViewDelegate>
    navigationDelegate;

// Hides or shows the manual fill password button.
@property(nonatomic) BOOL passwordButtonHidden;

// Hides or shows the manual fill credit card button.
@property(nonatomic) BOOL creditCardButtonHidden;

// Hides or shows the manual fill address button.
@property(nonatomic) BOOL addressButtonHidden;

// Enables or disables the next button if any.
@property(nonatomic) BOOL formInputNextButtonEnabled;

// Enables or disables the previous button if any.
@property(nonatomic) BOOL formInputPreviousButtonEnabled;

// Removes the animations on the custom keyboard view.
- (void)removeAnimationsOnKeyboardView;

// Removes the presented keyboard view and the input accessory view. Also clears
// the references to them, so nothing shows until a new custom view is passed.
- (void)restoreOriginalKeyboardView;

// Removes the presented keyboard view and the input accessory view until
// |continueCustomKeyboardView| is called.
- (void)pauseCustomKeyboardView;

// Adds the previously presented views to the keyboard. If they have not been
// reset.
- (void)continueCustomKeyboardView;

// Replace the keyboard accessory view with one showing the passed suggestions.
// And form navigation buttons if not an iPad (which already includes those).
// |isHardwareKeyboard| is true if a hardware keyboard is in use.
- (void)showAccessorySuggestions:(NSArray<FormSuggestion*>*)suggestions
                suggestionClient:(id<FormSuggestionClient>)suggestionClient
              isHardwareKeyboard:(BOOL)hardwareKeyboard;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_CONSUMER_H_
