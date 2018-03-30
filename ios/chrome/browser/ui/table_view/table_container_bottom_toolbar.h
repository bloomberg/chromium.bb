// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_CONTAINER_BOTTOM_TOOLBAR_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_CONTAINER_BOTTOM_TOOLBAR_H_

#import <UIKit/UIKit.h>

// TableContainer Bottom Toolbar that contains action buttons.
@interface TableContainerBottomToolbar : UIView
// Returns a Toolbar with leading and/or trailing buttons. If any of the
// parameters are nil or an empty string, a button will not be created.
- (instancetype)initWithLeadingButtonText:(NSString*)leadingButtonText
                       trailingButtonText:(NSString*)trailingButtonText
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Leading Toolbar button, nil if there's no leading button.
@property(nonatomic, strong, readonly) UIButton* leadingButton;
// Trailing Toolbar button, nil if there's no trailing button.
@property(nonatomic, strong, readonly) UIButton* trailingButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_CONTAINER_BOTTOM_TOOLBAR_H_
