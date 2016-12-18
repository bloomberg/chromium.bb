// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TODAY_EXTENSION_NOTIFICATION_CENTER_URL_BUTTON_H_
#define IOS_CHROME_TODAY_EXTENSION_NOTIFICATION_CENTER_URL_BUTTON_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/today_extension/notification_center_button.h"

typedef void (^URLActionBlock)(NSString*);

@interface NotificationCenterURLButton : NotificationCenterButton

// Create a button with contextual URL layout.
- (instancetype)initWithTitle:(NSString*)title
                          url:(NSString*)url
                         icon:(NSString*)icon
                    leftInset:(CGFloat)leftInset
                        block:(URLActionBlock)block;

// Show (or hide) a bottom separator below the button.
- (void)setSeparatorVisible:(BOOL)visible;

// Sets the title and the URL displayed in the button.
- (void)setTitle:(NSString*)title url:(NSString*)url;

@end

#endif  // IOS_CHROME_TODAY_EXTENSION_NOTIFICATION_CENTER_URL_BUTTON_H_
