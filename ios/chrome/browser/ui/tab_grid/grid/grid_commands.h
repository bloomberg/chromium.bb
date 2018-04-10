// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_COMMANDS_H_

// Commands issued to a model backing a grid UI.
@protocol GridCommands
// Tells the receiver to insert a new item at the end of the list.
- (void)addNewItem;
// Tells the receiver to insert a new item at |index|. It is an error to call
// this method with an |index| greater than the number of items in the model.
- (void)insertNewItemAtIndex:(NSUInteger)index;
// Tells the receiver to select the item with identifier |itemID|. If there is
// no item with that identifier, no change in selection should be made.
- (void)selectItemWithID:(NSString*)itemID;
// Tells the receiver to move the item with identifier |itemID| to |index|. If
// there is no item with that identifier, no move should be made. It is an error
// to pass a value for |index| outside of the bounds of the items array.
- (void)moveItemWithID:(NSString*)itemID toIndex:(NSUInteger)index;
// Tells the receiver to close the item with identifier |itemID|. If there is
// no item with that identifier, no item is closed.
- (void)closeItemWithID:(NSString*)itemID;
// Tells the receiver to close all items.
- (void)closeAllItems;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_COMMANDS_H_
