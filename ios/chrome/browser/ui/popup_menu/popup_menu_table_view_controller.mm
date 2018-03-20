// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PopupMenuTableViewController ()
@end

@implementation PopupMenuTableViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 0;
}

- (void)setPopupMenuItems:(NSArray<NSArray<TableViewItem*>*>*)items {
  [super loadModel];
  for (NSUInteger section = 0; section < items.count; section++) {
    NSInteger sectionIdentifier = kSectionIdentifierEnumZero + section;
    [self.tableViewModel addSectionWithIdentifier:sectionIdentifier];
    for (TableViewItem* item in items[section]) {
      [self.tableViewModel addItem:item
           toSectionWithIdentifier:sectionIdentifier];
    }
  }
  [self.tableView reloadData];
}

@end
