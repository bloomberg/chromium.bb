// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_KEYBOARD_ASSIST_ASSISTIVE_KEYBOARD_VIEWS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_KEYBOARD_ASSIST_ASSISTIVE_KEYBOARD_VIEWS_H_

#import <UIKit/UIKIt.h>

@protocol KeyboardAccessoryViewDelegate;

NSArray<UIButton*>* ToolbarAssistiveKeyboardLeadingButtons(
    id<KeyboardAccessoryViewDelegate> delegate);

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_KEYBOARD_ASSIST_ASSISTIVE_KEYBOARD_VIEWS_H_
