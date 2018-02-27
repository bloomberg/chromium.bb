// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/table_cell_catalog_view_controller.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierText = kSectionIdentifierEnumZero,
  SectionIdentifierURL,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeText = kItemTypeEnumZero,
  ItemTypeURLNoMetadata,
  ItemTypeURLWithTimestamp,
  ItemTypeURLWithSize,
};
}

@implementation TableCellCatalogViewController

- (instancetype)init {
  if ((self = [super initWithStyle:UITableViewStyleGrouped])) {
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Table Cell Catalog";
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = 56.0;

  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierText];
  [model addSectionWithIdentifier:SectionIdentifierURL];

  TableViewTextItem* textItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeText];
  textItem.text = @"Simple Text Cell";
  [model addItem:textItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewURLItem* item =
      [[TableViewURLItem alloc] initWithType:ItemTypeURLNoMetadata];
  item.favicon = [UIImage imageNamed:@"default_favicon"];
  item.title = @"Google Design";
  item.URL = @"design.google.com";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithTimestamp];
  item.favicon = [UIImage imageNamed:@"default_favicon"];
  item.title = @"Google";
  item.URL = @"google.com";
  item.metadata = @"3:42 PM";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithSize];
  item.favicon = [UIImage imageNamed:@"default_favicon"];
  item.title = @"World Series 2017: Houston Astros Defeat Someone Else";
  item.URL = @"m.bbc.com";
  item.metadata = @"176 KB";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];
}

@end
