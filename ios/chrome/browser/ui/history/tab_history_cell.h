// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_TAB_HISTORY_CELL_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_TAB_HISTORY_CELL_H_

#import <UIKit/UIKit.h>

@class CRWSessionEntry;

// Table cell used in TabHistoryViewController.
@interface TabHistoryCell : UICollectionViewCell
@property(strong, nonatomic) CRWSessionEntry* entry;
@property(weak, nonatomic, readonly) UILabel* titleLabel;
@end

// Header for a section of TabHistoryCells.
@interface TabHistorySectionHeader : UICollectionReusableView
@property(weak, nonatomic, readonly) UIImageView* iconView;
@property(weak, nonatomic, readonly) UIView* lineView;
@end

// Footer for a section of TabHistoryCells.
@interface TabHistorySectionFooter : UICollectionReusableView
@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_TAB_HISTORY_CELL_H_
