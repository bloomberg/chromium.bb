// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_WIDGET_EXTENSION_WIDGET_VIEW_H_
#define IOS_CHROME_WIDGET_EXTENSION_WIDGET_VIEW_H_

#import <UIKit/UIKit.h>

// Protocol to be implemented by targets for user actions coming from the widget
// view.
@protocol WidgetViewActionTarget

// Called when the user taps the fake omnibox.
- (void)openApp:(id)sender;

@end

// View for the widget. Shows a blinking cursor for a fake omnibox and calls the
// target when tapped.
@interface WidgetView : UIView

// Designated initializer, creates the widget view with a |target| for user
// actions.
- (instancetype)initWithActionTarget:(id<WidgetViewActionTarget>)target
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_WIDGET_EXTENSION_WIDGET_VIEW_H_
