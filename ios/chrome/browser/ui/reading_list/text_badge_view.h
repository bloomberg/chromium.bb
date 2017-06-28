// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_TEXT_BADGE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_TEXT_BADGE_VIEW_H_

#import <UIKit/UIKit.h>

// Pill-shaped view that displays white text.
@interface TextBadgeView : UIView

// Initialize the text badge with the given display text.
- (instancetype)initWithText:(NSString*)text NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_TEXT_BADGE_VIEW_H_
