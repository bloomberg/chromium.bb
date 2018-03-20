// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_mediator.h"

#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_tools_item.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PopupMenuMediator ()

// Items to be displayed in the popup menu.
@property(nonatomic, strong) NSArray<NSArray<TableViewItem*>*>* items;

// Type of this mediator.
@property(nonatomic, assign) PopupMenuType type;

@end

@implementation PopupMenuMediator

@synthesize items = _items;
@synthesize type = _type;

#pragma mark - Public

- (instancetype)initWithType:(PopupMenuType)type {
  self = [super init];
  if (self) {
    _type = type;
  }
  return self;
}

- (void)setUp {
  switch (self.type) {
    case PopupMenuTypeToolsMenu:
      [self createToolsMenuItem];
      break;
    case PopupMenuTypeNavigationForward:
      break;
    case PopupMenuTypeNavigationBackward:
      break;
    case PopupMenuTypeTabGrid:
      break;
    case PopupMenuTypeSearch:
      break;
  }
}

- (void)configurePopupMenu:(PopupMenuTableViewController*)popupMenu {
  [popupMenu setPopupMenuItems:self.items];
}

#pragma mark - Private

// Creates the menu items for the tools menu.
- (void)createToolsMenuItem {
  NSMutableArray* section1 = [NSMutableArray array];
  for (int i = 0; i < 3; i++) {
    PopupMenuToolsItem* item =
        [[PopupMenuToolsItem alloc] initWithType:kItemTypeEnumZero];
    item.title = [@"Item number:" stringByAppendingFormat:@"%i", i];
    [section1 addObject:item];
  }

  NSMutableArray* section2 = [NSMutableArray array];
  for (int i = 0; i < 5; i++) {
    PopupMenuToolsItem* item =
        [[PopupMenuToolsItem alloc] initWithType:kItemTypeEnumZero];
    item.title = [@"Item 2 number:" stringByAppendingFormat:@"%i", i];
    [section2 addObject:item];
  }

  self.items = @[ section1, section2 ];
}

@end
