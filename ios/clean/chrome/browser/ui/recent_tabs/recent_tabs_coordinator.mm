// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"

#include <memory>

#include "components/browser_sync/profile_sync_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/synced_session.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/synced_sessions_bridge.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/closed_tabs_observer_bridge.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_handset_view_controller.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_table_coordinator.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/clean/chrome/browser/ui/adaptor/application_commands_adaptor.h"
#import "ios/clean/chrome/browser/ui/adaptor/url_loader_adaptor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RecentTabsCoordinator ()<ClosedTabsObserving,
                                    SyncedSessionsObserver,
                                    RecentTabsHandsetViewControllerCommand,
                                    RecentTabsTableViewControllerDelegate> {
  std::unique_ptr<synced_sessions::SyncedSessionsObserverBridge>
      _syncedSessionsObserver;
  std::unique_ptr<recent_tabs::ClosedTabsObserverBridge> _closedTabsObserver;
}

@property(nonatomic, strong) UIViewController* viewController;
@property(nonatomic, strong) RecentTabsTableViewController* tableViewController;
@property(nonatomic, strong) URLLoaderAdaptor* loader;
@property(nonatomic, strong)
    ApplicationCommandsAdaptor* applicationCommandAdaptor;
@end

@implementation RecentTabsCoordinator
@synthesize mode = _mode;
@synthesize viewController = _viewController;
@synthesize tableViewController = _tableViewController;
@synthesize loader = _loader;
@synthesize applicationCommandAdaptor = _applicationCommandAdaptor;

- (void)start {
  if (self.started)
    return;

  DCHECK(self.mode != UNDEFINED);

  self.loader = [[URLLoaderAdaptor alloc] init];
  self.applicationCommandAdaptor = [[ApplicationCommandsAdaptor alloc] init];
  // HACK: Re-using old view controllers for now.
  self.tableViewController = [[RecentTabsTableViewController alloc]
      initWithBrowserState:self.browser->browser_state()
                    loader:self.loader
                dispatcher:self.applicationCommandAdaptor];
  self.tableViewController.delegate = self;

  if (self.mode == PRESENTED) {
    RecentTabsHandsetViewController* handsetViewController =
        [[RecentTabsHandsetViewController alloc]
            initWithViewController:self.tableViewController];
    handsetViewController.commandHandler = self;
    self.viewController = handsetViewController;
  } else {
    self.viewController = self.tableViewController;
  }
  [self startObservers];
  self.loader.viewControllerForAlert = self.viewController;
  self.applicationCommandAdaptor.viewControllerForAlert = self.viewController;

  [super start];
}

- (void)stop {
  [super stop];
  [self stopObservers];
  self.tableViewController = nil;
  self.viewController = nil;
}

#pragma mark - ClosedTabsObserving

- (void)tabRestoreServiceChanged:(sessions::TabRestoreService*)service {
  sessions::TabRestoreService* restoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(
          self.browser->browser_state());
  restoreService->LoadTabsFromLastSession();
  [self.tableViewController refreshRecentlyClosedTabs];
}

- (void)tabRestoreServiceDestroyed:(sessions::TabRestoreService*)service {
  [self.tableViewController setTabRestoreService:nullptr];
}

#pragma mark - SyncedSessionsObserver

- (void)reloadSessions {
  [self reloadSessionsData];
  [self refreshSessionsView];
}

- (void)onSyncStateChanged {
  [self refreshSessionsView];
}

#pragma mark - RecentTabsTableViewControllerDelegate

- (void)recentTabsTableViewContentMoved:(UITableView*)tableView {
  // TODO: update the shadow.
}

#pragma mark - RecentTabsHandsetViewControllerCommand

- (void)dismissRecentTabsWithCompletion:(void (^)())completion {
  [self stop];
}

#pragma mark - Private

- (void)startObservers {
  if (!_syncedSessionsObserver) {
    _syncedSessionsObserver.reset(
        new synced_sessions::SyncedSessionsObserverBridge(
            self, self.browser->browser_state()));
  }
  if (!_closedTabsObserver) {
    _closedTabsObserver.reset(new recent_tabs::ClosedTabsObserverBridge(self));
    sessions::TabRestoreService* restoreService =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(
            self.browser->browser_state());
    if (restoreService)
      restoreService->AddObserver(_closedTabsObserver.get());
    [self.tableViewController setTabRestoreService:restoreService];
  }
}

- (void)stopObservers {
  _syncedSessionsObserver.reset();

  if (_closedTabsObserver) {
    sessions::TabRestoreService* restoreService =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(
            self.browser->browser_state());
    if (restoreService) {
      restoreService->RemoveObserver(_closedTabsObserver.get());
    }
    _closedTabsObserver.reset();
  }
}

- (BOOL)isSignedIn {
  return _syncedSessionsObserver->IsSignedIn();
}

- (BOOL)isSyncTabsEnabled {
  DCHECK([self isSignedIn]);
  SyncSetupService* service = SyncSetupServiceFactory::GetForBrowserState(
      self.browser->browser_state());
  return !service->UserActionIsRequiredToHaveSyncWork();
}

// Returns whether this profile has any foreign sessions to sync.
- (SessionsSyncUserState)userSignedInState {
  if (![self isSignedIn])
    return SessionsSyncUserState::USER_SIGNED_OUT;
  if (![self isSyncTabsEnabled])
    return SessionsSyncUserState::USER_SIGNED_IN_SYNC_OFF;
  if ([self hasForeignSessions])
    return SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS;
  if (![self isSyncCompleted])
    return SessionsSyncUserState::USER_SIGNED_IN_SYNC_IN_PROGRESS;
  return SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_NO_SESSIONS;
}

- (BOOL)hasForeignSessions {
  browser_sync::ProfileSyncService* service =
      IOSChromeProfileSyncServiceFactory::GetForBrowserState(
          self.browser->browser_state());
  DCHECK(service);
  sync_sessions::OpenTabsUIDelegate* openTabs =
      service->GetOpenTabsUIDelegate();
  std::vector<const sync_sessions::SyncedSession*> sessions;
  return openTabs ? openTabs->GetAllForeignSessions(&sessions) : NO;
}

- (BOOL)isSyncCompleted {
  return _syncedSessionsObserver->IsFirstSyncCycleCompleted();
}

// Force a contact to the sync server to reload remote sessions.
- (void)reloadSessionsData {
  DVLOG(1) << "Triggering sync refresh for sessions datatype.";
  const syncer::ModelTypeSet types(syncer::SESSIONS);
  // Requests a sync refresh of the sessions for the current profile.
  IOSChromeProfileSyncServiceFactory::GetForBrowserState(
      self.browser->browser_state())
      ->TriggerRefresh(types);
}

// Reload the panel.
- (void)refreshSessionsView {
  [self.tableViewController refreshUserState:[self userSignedInState]];
}

@end
