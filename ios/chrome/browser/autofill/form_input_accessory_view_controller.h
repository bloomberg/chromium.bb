// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/form_input_accessory_consumer.h"

namespace autofill {
extern CGFloat const kInputAccessoryHeight;
}  // namespace autofill

@class ManualFillAccessoryViewController;

// Creates and manages a custom input accessory view while the user is
// interacting with a form. Also handles hiding and showing the default
// accessory view elements.
@interface FormInputAccessoryViewController
    : NSObject<FormInputAccessoryConsumer>

// The manual fill accessory view controller to add at the end of the
// suggestions.
@property(nonatomic, weak)
    ManualFillAccessoryViewController* manualFillAccessoryViewController;

// Presents a view above the keyboard.
- (void)presentView:(UIView*)view;

// Frees the manual fallback icons as the first option in the suggestions bar,
// and animates any suggestion back to their original position.
- (void)unlockManualFallbackView;

// Shows the manual fallback icons as the first option in the suggestions bar,
// and locks them in that position.
- (void)lockManualFallbackView;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_H_
