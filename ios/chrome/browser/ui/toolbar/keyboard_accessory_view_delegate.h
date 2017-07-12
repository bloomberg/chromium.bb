// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_KEYBOARD_ACCESSORY_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_KEYBOARD_ACCESSORY_VIEW_DELEGATE_H_

#import <UIKit/UIKIt.h>

// Delegate protocol for the KeyboardAccessoryView.
@protocol KeyboardAccessoryViewDelegate

// Notifies the delegate that the Voice Search button was pressed.
- (void)keyboardAccessoryVoiceSearchTouchDown:(UIView*)view;

// Notifies the delegate that a touch up occured in the the Voice Search button.
- (void)keyboardAccessoryVoiceSearchTouchUpInside:(UIView*)view;

// Notifies the delegate that a touch up occured in the the Camera Search
// button.
- (void)keyboardAccessoryCameraSearchTouchUpInside:(UIView*)view;

// Notifies the delegate that a key with the title |title| was pressed.
- (void)keyPressed:(NSString*)title;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_KEYBOARD_ACCESSORY_VIEW_DELEGATE_H_
