// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_KEYBOARD_ASSIST_TOOLBAR_ASSISTIVE_KEYBOARD_VIEWS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_KEYBOARD_ASSIST_TOOLBAR_ASSISTIVE_KEYBOARD_VIEWS_H_

#import <UIKit/UIKIt.h>

@protocol ToolbarAssistiveKeyboardDelegate;

// Returns the leading buttons of the assistive view.
NSArray<UIButton*>* ToolbarAssistiveKeyboardLeadingButtons(
    id<ToolbarAssistiveKeyboardDelegate> delegate);

// Adds a keyboard assistive view to |textField|. The assistive view shows a
// button to quickly enter |dotComTLD|, and the callbacks are handled via
// |delegate|.
// |dotComTLD| must not be nil.
void ConfigureAssistiveKeyboardViews(
    UITextField* textField,
    NSString* dotComTLD,
    id<ToolbarAssistiveKeyboardDelegate> delegate);

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_KEYBOARD_ASSIST_TOOLBAR_ASSISTIVE_KEYBOARD_VIEWS_H_
