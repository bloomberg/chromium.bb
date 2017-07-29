// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/tools_mediator.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_configuration.h"
#import "ios/clean/chrome/browser/ui/tools/tools_consumer.h"
#import "ios/clean/chrome/browser/ui/tools/tools_menu_item.h"
#import "ios/clean/chrome/browser/ui/tools/tools_menu_model.h"
#include "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolsMediator ()<CRWWebStateObserver>
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

@implementation ToolsMediator {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
}

@synthesize consumer = _consumer;
@synthesize toolsMenuConfiguration = _toolsMenuConfiguration;
@synthesize menuItems = _menuItems;
@synthesize webState = _webState;

- (instancetype)initWithConsumer:(id<ToolsConsumer>)consumer
                   configuration:(ToolsMenuConfiguration*)menuConfiguration {
  self = [super init];
  if (self) {
    self.toolsMenuConfiguration = menuConfiguration;
    self.consumer = consumer;
  }
  return self;
}

#pragma mark - Setters

- (void)setConsumer:(id<ToolsConsumer>)consumer {
  _consumer = consumer;
  [self populateMenuItems];
  [_consumer setToolsMenuItems:self.menuItems];
  [_consumer
      setDisplayOverflowControls:!self.toolsMenuConfiguration.isInTabSwitcher];
}

- (void)setWebState:(web::WebState*)webState {
  _webState = webState;
  // In the tab switcher, there's no overflow controls to update, so the
  // WebState isn't observed.
  if (!self.toolsMenuConfiguration.isInTabSwitcher) {
    _webStateObserver =
        base::MakeUnique<web::WebStateObserverBridge>(_webState, self);
    [self.consumer setIsLoading:self.webState->IsLoading()];
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateDidStartLoading:(web::WebState*)webState {
  [self.consumer setIsLoading:self.webState->IsLoading()];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self.consumer setIsLoading:self.webState->IsLoading()];
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
      menuItem.enabled = [self itemIsEnabledForCurrentConfiguration:modelItem];
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

// Returns true if item should be enabled on the current configuration.
- (BOOL)itemIsEnabledForCurrentConfiguration:(const MenuModelItem)modelItem {
  switch (modelItem.enabled) {
    case ItemEnabledAlways:
      return YES;
    case ItemEnabledNotInNTP:
      return !self.toolsMenuConfiguration.inNewTabPage;
    case ItemEnabledWhenOpenTabs:
      return !self.toolsMenuConfiguration.hasNoOpenedTabs;
    default:
      return YES;
  }
}

@end

// Private implementation for testing.
@implementation ToolsMediator (Testing)

- (NSArray*)menuItemsArray {
  return self.menuItems;
}

@end
