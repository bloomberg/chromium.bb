// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_coordinator.h"

#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/clean/chrome/browser/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/tab_model/tab_group.h"
#import "ios/clean/chrome/browser/ui/commands/settings_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_grid_commands.h"
#import "ios/clean/chrome/browser/ui/settings/settings_coordinator.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"
#import "ios/clean/chrome/browser/ui/tab_strip/tab_strip_container_coordinator.h"
#import "ios/clean/chrome/browser/web/web_mediator.h"
#import "ios/shared/chrome/browser/coordinator_context/coordinator_context.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridCoordinator ()<TabGridDataSource,
                                 SettingsCommands,
                                 TabCommands,
                                 TabGridCommands>
@property(nonatomic, strong) TabGroup* tabGroup;
@property(nonatomic, strong) TabGridViewController* viewController;
@property(nonatomic, weak) SettingsCoordinator* settingsCoordinator;
@end

@implementation TabGridCoordinator
@synthesize tabGroup = _tabGroup;
@synthesize viewController = _viewController;
@synthesize settingsCoordinator = _settingsCoordinator;

#pragma mark - Properties

- (void)setBrowserState:(ios::ChromeBrowserState*)browserState {
  [super setBrowserState:browserState];
  // PLACEHOLDER: Generate a tab group with four empty tabs.
  self.tabGroup =
      [TabGroup tabGroupWithEmptyTabCount:4 forBrowserState:self.browserState];
}

#pragma mark - BrowserCoordinator

- (void)start {
  self.viewController = [[TabGridViewController alloc] init];
  self.viewController.dataSource = self;
  self.viewController.settingsCommandHandler = self;
  self.viewController.tabCommandHandler = self;
  self.viewController.tabGridCommandHandler = self;

  // |baseViewController| is nullable, so this is by design a no-op if it hasn't
  // been set. This may be true in a unit test, or if this coordinator is being
  // used as a root coordinator.
  [self.baseViewController presentViewController:self.viewController
                                        animated:self.context.animated
                                      completion:nil];
}

#pragma mark - TabGridDataSource

- (NSUInteger)numberOfTabsInTabGrid {
  return self.tabGroup.count;
}

- (NSString*)titleAtIndex:(NSInteger)index {
  WebMediator* tab = [self.tabGroup webStateAtIndex:index];
  GURL url = tab.webState->GetVisibleURL();
  NSString* urlText = @"<New Tab>";
  if (!url.is_valid()) {
    urlText = base::SysUTF8ToNSString(url.spec());
  }
  return urlText;
}

#pragma mark - TabCommands

- (void)showTabAtIndexPath:(NSIndexPath*)indexPath {
  TabStripContainerCoordinator* tabCoordinator =
      [[TabStripContainerCoordinator alloc] init];
  tabCoordinator.webMediator = [self.tabGroup webStateAtIndex:indexPath.item];
  tabCoordinator.presentationKey = indexPath;
  [self addChildCoordinator:tabCoordinator];
  [tabCoordinator start];
}

#pragma mark - TabGridCommands

- (void)showTabGrid {
  // This object should only ever have at most one child.
  DCHECK_LE(self.children.count, 1UL);
  BrowserCoordinator* child = [self.children anyObject];
  [child stop];
  [self removeChildCoordinator:child];
}

#pragma mark - SettingsCommands

- (void)showSettings {
  SettingsCoordinator* settingsCoordinator = [[SettingsCoordinator alloc] init];
  settingsCoordinator.settingsCommandHandler = self;
  [self addOverlayCoordinator:settingsCoordinator];
  self.settingsCoordinator = settingsCoordinator;
  [settingsCoordinator start];
}

- (void)closeSettings {
  [self.settingsCoordinator stop];
  [self.settingsCoordinator.parentCoordinator
      removeChildCoordinator:self.settingsCoordinator];
  // self.settingsCoordinator should be presumed to be nil after this point.
}

#pragma mark - URLOpening

- (void)openURL:(NSURL*)URL {
  [self.overlayCoordinator stop];
  [self removeOverlayCoordinator];
  WebMediator* activeTab = [self.tabGroup activeTab];
  web::NavigationManager::WebLoadParams params(net::GURLWithNSURL(URL));
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  activeTab.webState->GetNavigationManager()->LoadURLWithParams(params);
  // Assume that either there is no child coordinator (tab) being displayed,
  // or that the child coordinator is displaying |activeTab|.
  if (!self.children.count) {
    NSUInteger index = [self.tabGroup indexOfWebState:activeTab];
    [self showTabAtIndexPath:[NSIndexPath indexPathForItem:index inSection:0]];
  }
}

@end
