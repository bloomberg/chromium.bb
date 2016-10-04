// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FAVICON_VIEW_H_
#define IOS_CHROME_BROWSER_UI_FAVICON_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/favicon/favicon_attributes.h"

namespace {
// Minimum width and height of favicon.
const CGFloat kFaviconMinSize = 16.0f;
// Default width and height of favicon.
const CGFloat kFaviconPreferredSize = 24.0f;
}  // namespace

@interface FaviconViewNew : UIView

// Configures this view with given attributes.
- (void)configureWithAttributes:(FaviconAttributes*)attributes;
// Sets monogram font.
- (void)setFont:(UIFont*)font;

@end

#endif  // IOS_CHROME_BROWSER_UI_FAVICON_VIEW_H_
