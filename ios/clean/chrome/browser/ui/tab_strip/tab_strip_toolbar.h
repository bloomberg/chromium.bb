// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_TOOLBAR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_TOOLBAR_H_

#import <UIKit/UIKit.h>

// A toolbar on the leading-most edge of the tab strip containing
// buttons to close the tab strip and enter incognito mode.
@interface TabStripToolbar : UICollectionReusableView
+ (NSString*)reuseIdentifier;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_TOOLBAR_H_
