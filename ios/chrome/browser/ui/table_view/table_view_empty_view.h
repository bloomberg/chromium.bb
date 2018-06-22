// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_EMPTY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_EMPTY_VIEW_H_

#import <UIKit/UIKit.h>

// Displays an UIImage on top of a message over a clearBackground.
@interface TableViewEmptyView : UIView

// Designated initializer.
- (instancetype)initWithFrame:(CGRect)frame
                      message:(NSString*)message
                        image:(UIImage*)image NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_EMPTY_VIEW_H_
