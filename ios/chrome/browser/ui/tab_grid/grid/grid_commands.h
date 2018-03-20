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
// Tells the receiver to select the item at |index|. It is an error to call this
// method with an |index| greater than the largest index in the model.
- (void)selectItemAtIndex:(NSUInteger)index;
// Tells the receiver to close the item at |index|. It is an error to call this
// method with an |index| greater than the largest index in the model.
- (void)closeItemAtIndex:(NSUInteger)index;
// Tells the receiver to close all items.
- (void)closeAllItems;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_COMMANDS_H_
