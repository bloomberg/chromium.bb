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
#import "ios/clean/chrome/browser/ui/commands/settings_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_grid_commands.h"
#import "ios/clean/chrome/browser/ui/settings/settings_coordinator.h"
#import "ios/clean/chrome/browser/ui/tab/tab_coordinator.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"
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
                                 TabGridCommands> {
  std::vector<std::unique_ptr<web::WebState>> _webStates;
  size_t _activeWebStateIndex;
}

@property(nonatomic, strong) TabGridViewController* viewController;
@property(nonatomic, weak) SettingsCoordinator* settingsCoordinator;
@end

@implementation TabGridCoordinator
@synthesize viewController = _viewController;
@synthesize settingsCoordinator = _settingsCoordinator;

#pragma mark - Properties

- (void)setBrowserState:(ios::ChromeBrowserState*)browserState {
  [super setBrowserState:browserState];

  for (int i = 0; i < 7; i++) {
    web::WebState::CreateParams webStateCreateParams(browserState);
    std::unique_ptr<web::WebState> webState =
        web::WebState::Create(webStateCreateParams);
    _webStates.push_back(std::move(webState));
  }
  _activeWebStateIndex = 0;
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
}

#pragma mark - TabGridDataSource

- (NSUInteger)numberOfTabsInTabGrid {
  return static_cast<NSUInteger>(_webStates.size());
}

- (NSString*)titleAtIndex:(NSInteger)index {
  size_t i = static_cast<size_t>(index);
  DCHECK(i < _webStates.size());
  web::WebState* webState = _webStates[i].get();
  GURL url = webState->GetVisibleURL();
  NSString* urlText = @"<New Tab>";
  if (url.is_valid()) {
    urlText = base::SysUTF8ToNSString(url.spec());
  }
  return urlText;
}

- (NSInteger)indexOfActiveTab {
  return static_cast<NSInteger>(_activeWebStateIndex);
}

#pragma mark - TabCommands

- (void)showTabAtIndexPath:(NSIndexPath*)indexPath {
  TabCoordinator* tabCoordinator = [[TabCoordinator alloc] init];
  size_t index = static_cast<size_t>(indexPath.item);
  DCHECK(index < _webStates.size());
  tabCoordinator.webState = _webStates[index].get();
  tabCoordinator.presentationKey = indexPath;
  [self addChildCoordinator:tabCoordinator];
  [tabCoordinator start];
  _activeWebStateIndex = index;
}

- (void)closeTabAtIndexPath:(NSIndexPath*)indexPath {
  size_t index = static_cast<size_t>(indexPath.item);
  DCHECK(index < _webStates.size());
  _webStates.erase(_webStates.begin() + index);
}

- (void)createNewTabAtIndexPath:(NSIndexPath*)indexPath {
  web::WebState::CreateParams webStateCreateParams(self.browserState);
  std::unique_ptr<web::WebState> webState =
      web::WebState::Create(webStateCreateParams);
  _webStates.push_back(std::move(webState));
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
  web::WebState* activeWebState = _webStates[_activeWebStateIndex].get();
  web::NavigationManager::WebLoadParams params(net::GURLWithNSURL(URL));
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  activeWebState->GetNavigationManager()->LoadURLWithParams(params);
  if (!self.children.count) {
    [self showTabAtIndexPath:[NSIndexPath
                                 indexPathForItem:static_cast<NSUInteger>(
                                                      _activeWebStateIndex)
                                        inSection:0]];
  }
}

@end
