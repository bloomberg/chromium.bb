// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_KEYBOARD_ASSIST_TOOLBAR_ASSISTIVE_KEYBOARD_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_KEYBOARD_ASSIST_TOOLBAR_ASSISTIVE_KEYBOARD_DELEGATE_H_

#import <UIKit/UIKIt.h>

// Delegate protocol for the KeyboardAccessoryView.
@protocol ToolbarAssistiveKeyboardDelegate

// Notifies the delegate that the Voice Search button was pressed.
- (void)keyboardAccessoryVoiceSearchTouchDown:(UIView*)view;

// Notifies the delegate that a touch up occurred in the the Voice Search
// button.
- (void)keyboardAccessoryVoiceSearchTouchUpInside:(UIView*)view;

// Notifies the delegate that a touch up occurred in the the Camera Search
// button.
- (void)keyboardAccessoryCameraSearchTouchUp;

// Notifies the delegate that a key with the title |title| was pressed.
- (void)keyPressed:(NSString*)title;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_KEYBOARD_ASSIST_TOOLBAR_ASSISTIVE_KEYBOARD_DELEGATE_H_
