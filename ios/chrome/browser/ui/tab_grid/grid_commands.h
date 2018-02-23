// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_COMMANDS_H_

// Commands issued to a model backing a grid UI.
@protocol GridCommands
// Tells the receiver to insert a new item at |index|. A negative |index| tells
// the receiver to insert a new item at the end of the list.
- (void)insertNewItemAtIndex:(NSInteger)index;
// Tells the receiver to select the item at |index|.
- (void)selectItemAtIndex:(NSUInteger)index;
// Tells the receiver to close the item at |index|.
- (void)closeItemAtIndex:(NSUInteger)index;
// Tells the receiver to close all items.
- (void)closeAllItems;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_COMMANDS_H_
