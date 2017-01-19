// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_WIDGET_EXTENSION_WIDGET_VIEW_H_
#define IOS_CHROME_WIDGET_EXTENSION_WIDGET_VIEW_H_

#import <UIKit/UIKit.h>

@interface WidgetView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_WIDGET_EXTENSION_WIDGET_VIEW_H_
