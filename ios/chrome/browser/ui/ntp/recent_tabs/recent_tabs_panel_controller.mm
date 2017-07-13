// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_panel_controller.h"

#include <memory>

#include "components/browser_sync/profile_sync_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/synced_session.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_bridges.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/sync/synced_sessions_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RecentTabsPanelController ()<SyncedSessionsObserver,
                                        RecentTabsTableViewControllerDelegate> {
  std::unique_ptr<synced_sessions::SyncedSessionsObserverBridge>
      _syncedSessionsObserver;
  std::unique_ptr<recent_tabs::ClosedTabsObserverBridge> _closedTabsObserver;
  SessionsSyncUserState _userState;
  RecentTabsTableViewController* _tableViewController;
  ios::ChromeBrowserState* _browserState;  // Weak.
}

// Return the user's current sign-in and chrome-sync state.
- (SessionsSyncUserState)userSignedInState;
// Utility functions for -userSignedInState so these can be mocked out
// easily for unit tests.
- (BOOL)isSignedIn;
- (BOOL)isSyncTabsEnabled;
- (BOOL)hasForeignSessions;
- (BOOL)isSyncCompleted;
// Reload the panel.
- (void)refreshSessionsView;
// Force a contact to the sync server to reload remote sessions.
- (void)reloadSessionsData;

@end

// The controller for RecentTabs panel that is added to the NewTabPage
// Instantiate a UITableView and a UITableViewController, and notifies the
// UITableViewController of any signed in state change.
@implementation RecentTabsPanelController

// Property declared in NewTabPagePanelProtocol.
@synthesize delegate = _delegate;

- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState {
  return [self initWithController:[[RecentTabsTableViewController alloc]
                                      initWithBrowserState:browserState
                                                    loader:loader]
                     browserState:browserState];
}

- (instancetype)initWithController:(RecentTabsTableViewController*)controller
                      browserState:(ios::ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    DCHECK(controller);
    DCHECK(browserState);
    _browserState = browserState;
    _tableViewController = controller;
    [_tableViewController setDelegate:self];
    [self initObservers];
    [self reloadSessions];
  }
  return self;
}

- (void)dealloc {
  [_tableViewController setDelegate:nil];
  [self deallocObservers];
}

- (void)initObservers {
  if (!_syncedSessionsObserver) {
    _syncedSessionsObserver.reset(
        new synced_sessions::SyncedSessionsObserverBridge(self, _browserState));
  }
  if (!_closedTabsObserver) {
    _closedTabsObserver.reset(new recent_tabs::ClosedTabsObserverBridge(self));
    sessions::TabRestoreService* restoreService =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(_browserState);
    if (restoreService)
      restoreService->AddObserver(_closedTabsObserver.get());
    [_tableViewController setTabRestoreService:restoreService];
  }
}

- (void)deallocObservers {
  _syncedSessionsObserver.reset();

  if (_closedTabsObserver) {
    sessions::TabRestoreService* restoreService =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(_browserState);
    if (restoreService) {
      restoreService->RemoveObserver(_closedTabsObserver.get());
    }
    _closedTabsObserver.reset();
  }
}

#pragma mark - Exposed to the SyncedSessionsObserver

- (void)reloadSessions {
  [self reloadSessionsData];
  [self refreshSessionsView];
}

- (void)onSyncStateChanged {
  [self refreshSessionsView];
}

#pragma mark - Exposed to the ClosedTabsObserverBridge

- (void)reloadClosedTabsList {
  sessions::TabRestoreService* restoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(_browserState);
  restoreService->LoadTabsFromLastSession();
  [_tableViewController refreshRecentlyClosedTabs];
}

- (void)tabRestoreServiceDestroyed {
  [_tableViewController setTabRestoreService:nullptr];
}

#pragma mark - NewTabPagePanelProtocol

- (void)dismissModals {
  [_tableViewController dismissModals];
}

- (void)dismissKeyboard {
}

- (void)reload {
  [self reloadSessions];
}

- (void)wasShown {
  [[_tableViewController tableView] reloadData];
  [self initObservers];
}

- (void)wasHidden {
  [self deallocObservers];
}

- (void)setScrollsToTop:(BOOL)enabled {
  [_tableViewController setScrollsToTop:enabled];
}

- (CGFloat)alphaForBottomShadow {
  UITableView* tableView = [_tableViewController tableView];
  CGFloat contentHeight = tableView.contentSize.height;
  CGFloat scrollViewHeight = tableView.frame.size.height;
  CGFloat offsetY = tableView.contentOffset.y;

  CGFloat pixelsBelowFrame = contentHeight - offsetY - scrollViewHeight;
  CGFloat alpha = pixelsBelowFrame / kNewTabPageDistanceToFadeShadow;
  alpha = MIN(MAX(alpha, 0), 1);
  return alpha;
}

- (UIView*)view {
  return [_tableViewController view];
}

- (UIViewController*)viewController {
  return _tableViewController;
}

#pragma mark - Private

- (BOOL)isSignedIn {
  SigninManager* signin_manager =
      ios::SigninManagerFactory::GetForBrowserState(_browserState);
  return signin_manager->IsAuthenticated();
}

- (BOOL)isSyncTabsEnabled {
  DCHECK([self isSignedIn]);
  SyncSetupService* service =
      SyncSetupServiceFactory::GetForBrowserState(_browserState);
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
      IOSChromeProfileSyncServiceFactory::GetForBrowserState(_browserState);
  DCHECK(service);
  sync_sessions::OpenTabsUIDelegate* openTabs =
      service->GetOpenTabsUIDelegate();
  std::vector<const sync_sessions::SyncedSession*> sessions;
  return openTabs ? openTabs->GetAllForeignSessions(&sessions) : NO;
}

- (BOOL)isSyncCompleted {
  return _syncedSessionsObserver->IsFirstSyncCycleCompleted();
}

- (void)reloadSessionsData {
  DVLOG(1) << "Triggering sync refresh for sessions datatype.";
  const syncer::ModelTypeSet types(syncer::SESSIONS);
  // Requests a sync refresh of the sessions for the current profile.
  IOSChromeProfileSyncServiceFactory::GetForBrowserState(_browserState)
      ->TriggerRefresh(types);
}

- (void)refreshSessionsView {
  [_tableViewController refreshUserState:[self userSignedInState]];
}

#pragma mark - RecentTabsTableViewControllerDelegate

- (void)recentTabsTableViewContentMoved:(UITableView*)tableView {
  [self.delegate updateNtpBarShadowForPanelController:self];
}

@end
