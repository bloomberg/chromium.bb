// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_tools_item.h"

#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PopupMenuToolsItem

@synthesize title = _title;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PopupMenuToolsCell class];
  }
  return self;
}

- (void)configureCell:(PopupMenuToolsCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setTitleText:self.title];
}

@end

#pragma mark - PopupMenuToolsCell

@interface PopupMenuToolsCell ()

// Title label for the cell.
@property(nonatomic, strong) UILabel* title;

@end

@implementation PopupMenuToolsCell

@synthesize title = _title;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _title = [[UILabel alloc] init];
    _title.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_title];
    AddSameConstraints(self.contentView, _title);
  }
  return self;
}

- (void)setTitleText:(NSString*)title {
  self.title.text = title;
}

@end
