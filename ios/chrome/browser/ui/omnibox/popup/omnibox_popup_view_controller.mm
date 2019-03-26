// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_base_view_controller+internal.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation OmniboxPopupViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = 44;

  [self.tableView registerClass:[OmniboxPopupRowCell class]
         forCellReuseIdentifier:OmniboxPopupRowCellReuseIdentifier];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [super scrollViewDidScroll:scrollView];

  // TODO(crbug.com/733650): Default to the dragging check once it's been tested
  // on trunk.
  if (!scrollView.dragging)
    return;

  [self.tableView deselectRowAtIndexPath:self.tableView.indexPathForSelectedRow
                                animated:NO];
}

#pragma mark - Table view data source

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  return UITableViewAutomaticDimension;
}

// Customize the appearance of table view cells.
- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);

  if (self.shortcutsEnabled && indexPath.row == 0 &&
      self.currentResult.count == 0) {
    return self.shortcutsCell;
  }

  DCHECK_LT((NSUInteger)indexPath.row, self.currentResult.count);
  OmniboxPopupRowCell* cell = [self.tableView
      dequeueReusableCellWithIdentifier:OmniboxPopupRowCellReuseIdentifier
                           forIndexPath:indexPath];
  [cell setupWithAutocompleteSuggestion:self.currentResult[indexPath.row]
                              incognito:self.incognito];

  return cell;
}

@end
