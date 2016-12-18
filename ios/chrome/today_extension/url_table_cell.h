// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TODAY_EXTENSION_URL_TABLE_CELL_H_
#define IOS_CHROME_TODAY_EXTENSION_URL_TABLE_CELL_H_

#import <UIKit/UIKit.h>

typedef void (^URLActionBlock)(NSString*);

@class PhysicalWebDevice;

// A UITableViewCell containing a NotificationCenterURLButton.
// The text and url can be set again to allow reuse of button.
// The icon cannot be changed, so buttons with different icons must use
// different |reuseIdentifier|.
@interface URLTableCell : UITableViewCell
- (instancetype)initWithTitle:(NSString*)title
                          url:(NSString*)url
                         icon:(NSString*)icon
                    leftInset:(CGFloat)leftInset
              reuseIdentifier:(NSString*)reuseIdentifier
                        block:(URLActionBlock)block;

// Sets the title and url of the button. The unescaped URL will be used as
// subtitle and passed as a parameter to the block.
- (void)setTitle:(NSString*)title url:(NSString*)url;

// Show (or hide) a bottom separator below the button.
// The separator is handled manually as it is not possible to hide only the last
// separator in UITableView.
- (void)setSeparatorVisible:(BOOL)visible;

@end

#endif  // IOS_CHROME_TODAY_EXTENSION_URL_TABLE_CELL_H_
