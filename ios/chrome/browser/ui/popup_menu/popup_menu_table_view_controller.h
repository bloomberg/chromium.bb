// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

// TableViewController for the popup menu.
@interface PopupMenuTableViewController : ChromeTableViewController

// Sets the |items| to be displayed by this Table View. Removes all the
// currently presented items.
- (void)setPopupMenuItems:(NSArray<NSArray<TableViewItem*>*>*)items;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_TABLE_VIEW_CONTROLLER_H_
