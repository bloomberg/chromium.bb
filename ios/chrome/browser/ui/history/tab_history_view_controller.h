// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_TAB_HISTORY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_TAB_HISTORY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// View controller for displaying a list of CRWSessionEntry objects in a table.
@interface TabHistoryViewController : UICollectionViewController

// TODO(crbug.com/546355): Convert this class to use an array of
//    NavigationEntries.
@property(nonatomic, strong) NSArray* sessionEntries;

// Returns the optimal height needed to display the session entries.
// The height returned is usually less than the |suggestedHeight| unless
// the last row of the table puts the height just over the |suggestedHeight|.
// If the session entries table is taller than the |suggestedHeight| by at least
// one row, the last visible session entry will be shown partially so the user
// can tell that the table is scrollable.
- (CGFloat)optimalHeight:(CGFloat)suggestedHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_TAB_HISTORY_VIEW_CONTROLLER_H_
