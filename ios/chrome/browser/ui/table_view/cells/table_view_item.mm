// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

#import "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TableViewItem

- (instancetype)initWithType:(NSInteger)type {
  if ((self = [super initWithType:type])) {
    self.cellClass = [UITableViewCell class];
  }
  return self;
}

- (void)configureCell:(UITableViewCell*)cell {
  DCHECK([cell class] == self.cellClass);
  cell.accessibilityTraits = self.accessibilityTraits;
  cell.accessibilityIdentifier = self.accessibilityIdentifier;
}

@end
