// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BARS_BOOKMARK_TOP_BAR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BARS_BOOKMARK_TOP_BAR_H_

#import <UIKit/UIKit.h>

@interface BookmarkTopBar : UIView

// All subviews should be added to |contentView|, which allows easy
// repositioning of the content to account for iOS 6 and iOS 7+ layout
// differences.
@property(nonatomic, readonly) UIView* contentView;

// The height that this view's content view will be instantiated at. This is
// independent of the status bar.
+ (CGFloat)expectedContentViewHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BARS_BOOKMARK_TOP_BAR_H_
