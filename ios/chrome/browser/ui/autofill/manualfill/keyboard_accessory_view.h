// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUALFILL_KEYBOARD_ACCESSORY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUALFILL_KEYBOARD_ACCESSORY_VIEW_H_

#import <UIKit/UIKit.h>

// Protocol to handle user interactions in a ManualFillKeyboardAccessoryView.
@protocol ManualFillKeyboardAccessoryViewDelegate

// Invoked after the user touches the `accounts` button.
- (void)accountButtonPressed;

// Invoked after the user touches the `credit cards` button.
- (void)cardButtonPressed;

// Invoked after the user touches the `passwords` button.
- (void)passwordButtonPressed;

@end

// This view contains the icons to activate "Manual Fill". It is meant to be
// shown above the keyboard on iPhone and above the manual fill view.
@interface ManualFillKeyboardAccessoryView : UIView

// Instances an object with the desired delegate.
//
// @param delegate The delegate for this object.
// @return A fresh object with the passed delegate.
- (instancetype)initWithDelegate:
    (id<ManualFillKeyboardAccessoryViewDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// Unavailable. Use `initWithDelegate:`.
- (instancetype)init NS_UNAVAILABLE;

// Unavailable. Use `initWithDelegate:`.
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Unavailable. Use `initWithDelegate:`.
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

// Unavailable. Use `initWithDelegate:`.
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUALFILL_KEYBOARD_ACCESSORY_VIEW_H_
