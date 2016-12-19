// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EXTENDED_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EXTENDED_BUTTON_H_

#import <UIKit/UIKit.h>

// This UIButton subclass extends the tappable area.
@interface BookmarkExtendedButton : UIButton
// Positive values extend tappable area of the button.
// The default value is Apple's minimum recommended touch area, if the button is
// too small.
@property(nonatomic, assign) UIEdgeInsets extendedEdges;
@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EXTENDED_BUTTON_H_
