// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_EMPTY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_EMPTY_VIEW_H_

#import <UIKit/UIKit.h>

// Displays an UIImage on top of a message over a clearBackground.
@interface TableViewEmptyView : UIView

// Designated initializer for a view that displays |message| with default
// styling and |image| above the message.
- (instancetype)initWithFrame:(CGRect)frame
                      message:(NSString*)message
                        image:(UIImage*)image NS_DESIGNATED_INITIALIZER;
// Designated initializer for a view that displays an attributed |message| and
// |image| above the message.
- (instancetype)initWithFrame:(CGRect)frame
            attributedMessage:(NSAttributedString*)message
                        image:(UIImage*)image NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

// The accessibility label to use for the message label.  Default value is the
// message iteself.
@property(nonatomic, strong) NSString* messageAccessibilityLabel;

// Insets of the inner ScrollView.
@property(nonatomic, assign) UIEdgeInsets scrollViewContentInsets;

// The empty view's accessibility identifier.
+ (NSString*)accessibilityIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_EMPTY_VIEW_H_
