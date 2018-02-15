// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/table_view_model.h"

@class TableViewItem;

// Chrome-specific TableViewController.
@interface ChromeTableViewController : UITableViewController

// The model of this controller.
@property(nonatomic, readonly, strong)
    TableViewModel<TableViewItem*>* tableViewModel;

// Initializes the collection view model. Must be called by subclasses if they
// override this method in order to get a clean tableViewModel.
- (void)loadModel NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_CONTROLLER_H_
