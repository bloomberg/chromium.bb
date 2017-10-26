// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_H_

#import <UIKit/UIKit.h>

@protocol FormInputAccessoryViewDelegate;

// Subview of the accessory view for web forms. Shows a custom view with form
// navigation controls above the keyboard. Subclassed to enable input clicks by
// way of the playInputClick method.
@interface FormInputAccessoryView : UIView<UIInputViewAudioFeedback>

// Initializes with |delegate|.
- (instancetype)initWithDelegate:(id<FormInputAccessoryViewDelegate>)delegate;

// Initializes with |frame| to show |customView|. Navigation controls are not
// shown.
- (instancetype)initWithFrame:(CGRect)frame customView:(UIView*)customView;

// Initializes the view with the given |customView|.
// If the size of |rightFrame| is non-zero, the view will be split into two
// parts with |leftFrame| and |rightFrame|. Otherwise the Autofill view will
// be shown in |leftFrame|.
- (void)initializeViewWithCustomView:(UIView*)customView
                           leftFrame:(CGRect)leftFrame
                          rightFrame:(CGRect)rightFrame;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_H_
