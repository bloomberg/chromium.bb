// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_ELEVATED_TOOLBAR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_ELEVATED_TOOLBAR_H_

#import <UIKit/UIKit.h>

@class MDCButton;

// A class containing one button that has a Material shadow.
@interface BookmarksElevatedToolbar : UIView

@property(nonatomic, assign) CGFloat shadowElevation;

- (void)setButton:(MDCButton*)button;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_ELEVATED_TOOLBAR_H_
