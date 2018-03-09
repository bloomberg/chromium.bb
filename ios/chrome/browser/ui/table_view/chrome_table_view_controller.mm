// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromeTableViewController
@synthesize tableViewModel = _tableViewModel;

#pragma mark - Public Interface

- (void)loadModel {
  _tableViewModel = [[TableViewModel alloc] init];
}

#pragma mark - ViewLifeCycle

- (void)viewDidLoad {
  [super viewDidLoad];
  [self.tableView setBackgroundColor:[UIColor whiteColor]];
  [self.tableView setSeparatorColor:[UIColor grayColor]];
  [self.tableView setSeparatorInset:UIEdgeInsetsMake(0, 56, 0, 0)];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  Class cellClass = [item cellClass];
  NSString* reuseIdentifier = NSStringFromClass(cellClass);
  [self.tableView registerClass:cellClass
         forCellReuseIdentifier:reuseIdentifier];
  UITableViewCell* cell =
      [self.tableView dequeueReusableCellWithIdentifier:reuseIdentifier
                                           forIndexPath:indexPath];
  [item configureCell:cell];

  return cell;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return [self.tableViewModel numberOfItemsInSection:section];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return [self.tableViewModel numberOfSections];
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  TableViewHeaderFooterItem* item =
      [self.tableViewModel headerForSection:section];
  if (!item)
    return [[UIView alloc] initWithFrame:CGRectZero];
  Class headerFooterClass = [item cellClass];
  NSString* reuseIdentifier = NSStringFromClass(headerFooterClass);
  [self.tableView registerClass:headerFooterClass
      forHeaderFooterViewReuseIdentifier:reuseIdentifier];
  UITableViewHeaderFooterView* view = [self.tableView
      dequeueReusableHeaderFooterViewWithIdentifier:reuseIdentifier];
  [item configureHeaderFooterView:view];
  return view;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  TableViewHeaderFooterItem* item =
      [self.tableViewModel footerForSection:section];
  if (!item)
    return [[UIView alloc] initWithFrame:CGRectZero];
  Class headerFooterClass = [item cellClass];
  NSString* reuseIdentifier = NSStringFromClass(headerFooterClass);
  [self.tableView registerClass:headerFooterClass
      forHeaderFooterViewReuseIdentifier:reuseIdentifier];
  UITableViewHeaderFooterView* view = [self.tableView
      dequeueReusableHeaderFooterViewWithIdentifier:reuseIdentifier];
  [item configureHeaderFooterView:view];
  return view;
}

@end
