// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromeTableViewController
@synthesize styler = _styler;
@synthesize tableViewModel = _tableViewModel;

#pragma mark - Public Interface

- (instancetype)initWithStyle:(UITableViewStyle)style {
  if ((self = [super initWithStyle:style])) {
    _styler = [[ChromeTableViewStyler alloc] init];
  }
  return self;
}

- (void)loadModel {
  _tableViewModel = [[TableViewModel alloc] init];
}

- (void)reconfigureCellsForItems:(NSArray*)items {
  for (TableViewItem* item in items) {
    NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:item];
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];

    // |cell| may be nil if the row is not currently on screen.
    if (cell) {
      [item configureCell:cell withStyler:self.styler];
    }
  }
}

#pragma mark - ViewLifeCycle

- (void)viewDidLoad {
  [super viewDidLoad];
  [self.tableView setBackgroundColor:self.styler.tableViewBackgroundColor];
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
  [item configureCell:cell withStyler:self.styler];

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
  [item configureHeaderFooterView:view withStyler:self.styler];
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
  [item configureHeaderFooterView:view withStyler:self.styler];
  return view;
}

@end
