// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_KEYBOARD_OBSERVER_HELPER_H_
#define IOS_CHROME_BROWSER_UI_UTIL_KEYBOARD_OBSERVER_HELPER_H_

#import <UIKit/UIKit.h>

// Delegate informed about the visible/hidden state of the keyboard.
@protocol KeyboardObserverHelperConsumer <NSObject>

// Indicates that |UIKeyboardWillShowNotification| was posted. And informs if a
// physical keyboard is attached. On iPad also considers
// |UIKeyboardDidChangeFrameNotification| since when the keyboard is not docked,
// |UIKeyboardWillShowNotification| isn't posted.
- (void)keyboardWillShowWithHardwareKeyboardAttached:(BOOL)isHardwareKeyboard;

// Indicates that |UIKeyboardWillHideNotification| was posted but the keyboard
// was not hidden. For example, this can happen when jumping between fields.
- (void)keyboardDidStayOnScreen;

// Indicates that |UIKeyboardWillHideNotification| was posted and the keyboard
// was actually dismissed.
- (void)keyboardDidHide;

@end

// Helper to observe the keyboard and report updates.
@interface KeyboardObserverHelper : NSObject

// Flag that indicates if the keyboard is on screen.
@property(nonatomic, readonly, getter=isKeyboardOnScreen) BOOL keyboardOnScreen;

// The consumer to inform of the keyboard state changes.
@property(nonatomic, weak) id<KeyboardObserverHelperConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_UTIL_KEYBOARD_OBSERVER_HELPER_H_
