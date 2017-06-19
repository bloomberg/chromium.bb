// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_TAB_STRIP_ACTIONS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_TAB_STRIP_ACTIONS_H_

#import <Foundation/Foundation.h>

// Target/Action methods relating to the tab strip.
@protocol TabStripActions
@optional
// Hides the tab strip.
- (void)hideTabStrip:(id)sender;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_TAB_STRIP_ACTIONS_H_
