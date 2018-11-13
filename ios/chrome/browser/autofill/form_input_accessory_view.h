// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_H_

#import <UIKit/UIKit.h>

@protocol FormInputAccessoryViewDelegate;

// Subview of the accessory view for web forms. Shows a custom view with form
// navigation controls above the keyboard. Enables input clicks by way of the
// playInputClick method.
@interface FormInputAccessoryView : UIView<UIInputViewAudioFeedback>

// Sets up the view with the given |leadingView|. Navigation controls are shown
// on the trailing side and use |delegate| for actions.
- (void)setUpWithLeadingView:(UIView*)leadingView
          navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate;

// Sets up the view with the given |leadingView|. Navigation controls are
// replaced with |customTrailingView|.
- (void)setUpWithLeadingView:(UIView*)leadingView
          customTrailingView:(UIView*)customTrailingView;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_H_
