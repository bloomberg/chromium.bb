// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/tools_mediator.h"

#include "base/macros.h"
#import "ios/clean/chrome/browser/ui/tools/tools_consumer.h"
#import "ios/clean/chrome/browser/ui/tools/tools_menu_item.h"
#import "ios/clean/chrome/browser/ui/tools/tools_menu_model.h"
#import "ios/shared/chrome/browser/ui/tools_menu/tools_menu_configuration.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolsMediator ()
@property(nonatomic, strong) id<ToolsConsumer> consumer;
@property(nonatomic, strong) ToolsMenuConfiguration* toolsMenuConfiguration;
@property(nonatomic, strong) NSMutableArray* menuItems;
// Populates the menuItems array by filtering the model data source based on the
// current ToolsMenuConfiguration.
- (void)populateMenuItems;
// Checks if a specific Menu Item should be visible for the current
// configuration.
- (BOOL)itemIsVisibleForCurrentConfiguration:(const MenuModelItem)modelItem;

@end

@implementation ToolsMediator

@synthesize consumer = _consumer;
@synthesize toolsMenuConfiguration = _toolsMenuConfiguration;
@synthesize menuItems = _menuItems;

- (instancetype)initWithConsumer:(id<ToolsConsumer>)consumer
                andConfiguration:(ToolsMenuConfiguration*)menuConfiguration {
  self = [super init];
  if (self) {
    self.toolsMenuConfiguration = menuConfiguration;
    self.consumer = consumer;
  }
  return self;
}

- (void)setConsumer:(id<ToolsConsumer>)consumer {
  _consumer = consumer;

  [self populateMenuItems];

  [_consumer setToolsMenuItems:self.menuItems];
  [_consumer
      setDisplayOverflowControls:!self.toolsMenuConfiguration.isInTabSwitcher];
}

#pragma mark - Private Methods

- (void)populateMenuItems {
  self.menuItems = [NSMutableArray array];

  for (size_t i = 0; i < arraysize(itemsModelList); ++i) {
    const MenuModelItem& modelItem = itemsModelList[i];

    if ([self itemIsVisibleForCurrentConfiguration:modelItem]) {
      ToolsMenuItem* menuItem = [[ToolsMenuItem alloc] init];
      menuItem.title = l10n_util::GetNSStringWithFixup(modelItem.title_id);
      menuItem.action = NSSelectorFromString(modelItem.selector);
      [self.menuItems addObject:menuItem];
    }
  }
}

- (BOOL)itemIsVisibleForCurrentConfiguration:(const MenuModelItem)modelItem {
  // First check if the item should be visible on the current Incognito
  // state.
  switch (modelItem.visibility) {
    case ItemVisibleIncognitoOnly:
      if (!self.toolsMenuConfiguration.isInIncognito) {
        return NO;
      }
      break;
    case ItemVisibleNotIncognitoOnly:
      if (self.toolsMenuConfiguration.isInIncognito) {
        return NO;
      }
      break;
    case ItemVisibleAlways:
      break;
    default:
      return NO;
  }

  // Then check if the item should be visibile for the current Toolbar state.
  switch (modelItem.toolbar_types) {
    case ToolbarTypeNone:
      return NO;
    case ToolbarTypeSwitcher:
      return self.toolsMenuConfiguration.isInTabSwitcher;
    case ToolbarTypeWeb:
      return !self.toolsMenuConfiguration.isInTabSwitcher;
    case ToolbarTypeAll:
      return YES;
    default:
      return NO;
  }
}

@end

// Private implementation for testing.
@implementation ToolsMediator (Testing)

- (NSArray*)menuItemsArray {
  return self.menuItems;
}

@end
