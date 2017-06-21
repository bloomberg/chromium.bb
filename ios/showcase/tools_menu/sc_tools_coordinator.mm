// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/tools_menu/sc_tools_coordinator.h"

#include "base/macros.h"
#import "ios/clean/chrome/browser/ui/commands/find_in_page_visibility_commands.h"
#import "ios/clean/chrome/browser/ui/commands/navigation_commands.h"
#import "ios/clean/chrome/browser/ui/commands/settings_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tools_menu_commands.h"
#import "ios/clean/chrome/browser/ui/tools/menu_view_controller.h"
#import "ios/clean/chrome/browser/ui/tools/tools_menu_item.h"
#import "ios/clean/chrome/browser/ui/tools/tools_menu_model.h"
#import "ios/showcase/common/protocol_alerter.h"
#include "ui/base/l10n/l10n_util.h"

@interface SCToolsCoordinator ()
@property(nonatomic, strong) ProtocolAlerter* alerter;
@end

@implementation SCToolsCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize alerter = _alerter;

#pragma mark - BrowserCoordinator

- (void)start {
  self.alerter = [[ProtocolAlerter alloc] initWithProtocols:@[
    @protocol(FindInPageVisibilityCommands), @protocol(NavigationCommands),
    @protocol(ToolsMenuCommands), @protocol(SettingsCommands)
  ]];
  self.alerter.baseViewController = self.baseViewController;

  MenuViewController* viewController = [[MenuViewController alloc] init];
  viewController.modalPresentationStyle = UIModalPresentationCustom;
  viewController.dispatcher =
      static_cast<id<FindInPageVisibilityCommands, NavigationCommands,
                     ToolsMenuCommands, SettingsCommands>>(self.alerter);

  NSMutableArray* menuItems = [NSMutableArray array];

  // Load the full model into the VC.
  for (size_t i = 0; i < arraysize(itemsModelList); ++i) {
    const MenuModelItem& modelItem = itemsModelList[i];
    ToolsMenuItem* menuItem = [[ToolsMenuItem alloc] init];
    menuItem.title = l10n_util::GetNSStringWithFixup(modelItem.title_id);
    menuItem.action = NSSelectorFromString(modelItem.selector);
    [menuItems addObject:menuItem];
  }
  [viewController setToolsMenuItems:menuItems];

  // The Overflow controls will only be displayed on CompactWidth SizeClasses.
  [viewController setDisplayOverflowControls:YES];

  // Since the close MenuButton is always located on the top right corner,
  // set the navigation translucency to NO so it doesn't cover the button.
  self.baseViewController.navigationBar.translucent = NO;

  [self.baseViewController pushViewController:viewController animated:YES];
}

@end
