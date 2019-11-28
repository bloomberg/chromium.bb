// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ACTION_CELL_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ACTION_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// Wrapper to use Action Cells in Chrome Table Views.
@interface ManualFillActionItem : TableViewItem

- (instancetype)initWithTitle:(NSString*)title
                       action:(void (^)(void))action NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

// Set enable to NO to create a message line cell.
@property(nonatomic, assign) BOOL enabled;

// Wheter to show a gray, separator line, at the bottom of a cell, or not.
@property(nonatomic, assign) BOOL showSeparator;

@end

// A table view cell which contains a button and holds an action block, which
// is called when the button is touched.
@interface ManualFillActionCell : TableViewCell
// Updates the cell with the passed title and action block.
- (void)setUpWithTitle:(NSString*)title
       accessibilityID:(NSString*)accessibilityID
                action:(void (^)(void))action
               enabled:(BOOL)enabled
         showSeparator:(BOOL)showSeparator;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ACTION_CELL_H_
