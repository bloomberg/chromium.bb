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
#import "ios/clean/chrome/browser/model/browser.h"
#import "ios/clean/chrome/browser/ui/commands/settings_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_grid_commands.h"
#import "ios/clean/chrome/browser/ui/settings/settings_coordinator.h"
#import "ios/clean/chrome/browser/ui/tab/tab_coordinator.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"
#import "ios/shared/chrome/browser/coordinator_context/coordinator_context.h"
#import "ios/shared/chrome/browser/tabs/web_state_list.h"
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
@property(nonatomic, strong) TabGridViewController* viewController;
@property(nonatomic, weak) SettingsCoordinator* settingsCoordinator;
@property(nonatomic, readonly) WebStateList& webStateList;
@end

@implementation TabGridCoordinator
@synthesize viewController = _viewController;
@synthesize settingsCoordinator = _settingsCoordinator;

#pragma mark - Properties

- (void)setBrowser:(Browser*)browser {
  [super setBrowser:browser];

  for (int i = 0; i < 7; i++) {
    web::WebState::CreateParams webStateCreateParams(browser->browser_state());
    std::unique_ptr<web::WebState> webState =
        web::WebState::Create(webStateCreateParams);
    self.webStateList.InsertWebState(0, webState.release(), nullptr);
  }
  self.webStateList.ActivateWebStateAt(0);
}

- (WebStateList&)webStateList {
  return self.browser->web_state_list();
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
  [self.context.baseViewController presentViewController:self.viewController
                                                animated:self.context.animated
                                              completion:nil];
  [super start];
}

#pragma mark - TabGridDataSource

- (int)numberOfTabsInTabGrid {
  return self.webStateList.count();
}

- (NSString*)titleAtIndex:(int)index {
  GURL url = self.webStateList.GetWebStateAt(index)->GetVisibleURL();
  NSString* urlText = @"<New Tab>";
  if (url.is_valid()) {
    urlText = base::SysUTF8ToNSString(url.spec());
  }
  return urlText;
}

- (int)indexOfActiveTab {
  return self.webStateList.active_index();
}

#pragma mark - TabCommands

- (void)showTabAtIndexPath:(NSIndexPath*)indexPath {
  TabCoordinator* tabCoordinator = [[TabCoordinator alloc] init];
  DCHECK_LE(indexPath.item, INT_MAX);
  int index = static_cast<int>(indexPath.item);
  self.webStateList.ActivateWebStateAt(index);
  // PLACEHOLDER: The tab coordinator should be able to get the active webState
  // on its own.
  tabCoordinator.webState = self.webStateList.GetWebStateAt(index);
  tabCoordinator.presentationKey = indexPath;
  [self addChildCoordinator:tabCoordinator];
  [tabCoordinator start];
}

- (void)closeTabAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_LE(indexPath.item, INT_MAX);
  int index = static_cast<int>(indexPath.item);
  std::unique_ptr<web::WebState> closedWebState(
      self.webStateList.DetachWebStateAt(index));
}

- (void)createNewTabAtIndexPath:(NSIndexPath*)indexPath {
  web::WebState::CreateParams webStateCreateParams(
      self.browser->browser_state());
  std::unique_ptr<web::WebState> webState =
      web::WebState::Create(webStateCreateParams);
  self.webStateList.InsertWebState(self.webStateList.count(),
                                   webState.release(), nullptr);
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
  if (self.webStateList.active_index() == WebStateList::kInvalidIndex) {
    return;
  }
  [self.overlayCoordinator stop];
  [self removeOverlayCoordinator];
  web::WebState* activeWebState = self.webStateList.GetActiveWebState();
  web::NavigationManager::WebLoadParams params(net::GURLWithNSURL(URL));
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  activeWebState->GetNavigationManager()->LoadURLWithParams(params);
  if (!self.children.count) {
    [self
        showTabAtIndexPath:[NSIndexPath indexPathForItem:[self indexOfActiveTab]
                                               inSection:0]];
  }
}

@end
