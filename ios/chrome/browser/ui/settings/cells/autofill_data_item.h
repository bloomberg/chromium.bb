// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_AUTOFILL_DATA_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_AUTOFILL_DATA_ITEM_H_

#include <string>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// Item for autofill profile (address) or credit card.
@interface AutofillDataItem : TableViewItem

// Only deletable items will enter edit mode.
@property(nonatomic, assign, getter=isDeletable) BOOL deletable;

// The GUID used by the PersonalDataManager to identify data elements (e.g.
// profiles and credit cards).
@property(nonatomic, assign) std::string GUID;

@property(nonatomic, copy) NSString* text;
@property(nonatomic, copy) NSString* leadingDetailText;
@property(nonatomic, copy) NSString* trailingDetailText;

@end

// Cell for autofill data with two leading text labels and one trailing text
// label.
@interface AutofillDataCell : UITableViewCell

@property(nonatomic, readonly, strong) UILabel* textLabel;
@property(nonatomic, readonly, strong) UILabel* leadingDetailTextLabel;
@property(nonatomic, readonly, strong) UILabel* trailingDetailTextLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_AUTOFILL_DATA_ITEM_H_
