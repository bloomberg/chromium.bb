// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#import "base/ios/crb_protocol_observers.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/ios/ios_live_tab.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/chrome_url_util.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_service.h"
#import "ios/chrome/browser/sessions/session_window.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#include "ios/chrome/browser/tab_parenting_global_observer.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/tabs/tab_model_order_controller.h"
#import "ios/chrome/browser/tabs/tab_model_synced_window_delegate.h"
#import "ios/chrome/browser/xcallback_parameters.h"
#import "ios/web/navigation/crw_session_certificate_policy_manager.h"
#import "ios/web/navigation/crw_session_controller.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/certificate_policy_cache.h"
#include "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#include "url/gurl.h"

NSString* const kTabModelTabWillStartLoadingNotification =
    @"kTabModelTabWillStartLoadingNotification";
NSString* const kTabModelUserNavigatedNotification = @"kTabModelUserNavigation";
NSString* const kTabModelTabDidStartLoadingNotification =
    @"kTabModelTabDidStartLoadingNotification";
NSString* const kTabModelTabDidFinishLoadingNotification =
    @"kTabModelTabDidFinishLoadingNotification";
NSString* const kTabModelAllTabsDidCloseNotification =
    @"kTabModelAllTabsDidCloseNotification";
NSString* const kTabModelTabDeselectedNotification =
    @"kTabModelTabDeselectedNotification";
NSString* const kTabModelNewTabWillOpenNotification =
    @"kTabModelNewTabWillOpenNotification";
NSString* const kTabModelTabKey = @"tab";
NSString* const kTabModelPageLoadSuccess = @"pageLoadSuccess";
NSString* const kTabModelOpenInBackgroundKey = @"shouldOpenInBackground";

namespace {

// Updates CRWSessionCertificatePolicyManager's certificate policy cache.
void UpdateCertificatePolicyCacheFromWebState(
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    web::WebState* web_state) {
  DCHECK(web_state);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  // TODO(crbug.com/454984): Remove CRWSessionController usage once certificate
  // policy manager is moved to NavigationManager.
  CRWSessionController* controller = static_cast<web::WebStateImpl*>(web_state)
                                         ->GetNavigationManagerImpl()
                                         .GetSessionController();
  [[controller sessionCertificatePolicyManager]
      updateCertificatePolicyCache:policy_cache];
}

// Populates the certificate policy cache based on the WebStates of |tab_model|.
void RestoreCertificatePolicyCacheFromModel(
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    TabModel* tab_model) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  for (Tab* tab in tab_model)
    UpdateCertificatePolicyCacheFromWebState(policy_cache, tab.webState);
}

// Scrubs the certificate policy cache of all certificates policies except
// those for the current entries in |tab_model|.
void CleanCertificatePolicyCache(
    base::CancelableTaskTracker* task_tracker,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    TabModel* tab_model) {
  DCHECK(tab_model);
  DCHECK(policy_cache);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  task_tracker->PostTaskAndReply(
      task_runner.get(), FROM_HERE,
      base::Bind(&web::CertificatePolicyCache::ClearCertificatePolicies,
                 policy_cache),
      base::Bind(&RestoreCertificatePolicyCacheFromModel, policy_cache,
                 base::Unretained(tab_model)));
}

}  // anonymous namespace

@interface TabModelObservers : CRBProtocolObservers<TabModelObserver>
@end
@implementation TabModelObservers
@end

@interface TabModel ()<TabUsageRecorderDelegate> {
  // Array of |Tab| objects.
  base::scoped_nsobject<NSMutableArray> _tabs;
  // Maintains policy for where new tabs go and the selection when a tab
  // is removed.
  base::scoped_nsobject<TabModelOrderController> _orderController;
  // The delegate for sync.
  std::unique_ptr<TabModelSyncedWindowDelegate> _syncedWindowDelegate;
  // Currently selected tab. May be nil.
  base::WeakNSObject<Tab> _currentTab;

  // Counters for metrics.
  int _openedTabCount;
  int _closedTabCount;
  int _newTabCount;

  // Backs up property with the same name.
  std::unique_ptr<TabUsageRecorder> _tabUsageRecorder;
  // Backs up property with the same name.
  const SessionID _sessionID;
  // Saves session's state.
  base::scoped_nsobject<SessionServiceIOS> _sessionService;
  // List of TabModelObservers.
  base::scoped_nsobject<TabModelObservers> _observers;

  // Used to ensure thread-safety of the certificate policy management code.
  base::CancelableTaskTracker _clearPoliciesTaskTracker;
}

// Session window for the contents of the tab model.
@property(nonatomic, readonly) SessionWindowIOS* windowForSavingSession;

// Returns YES if tab URL host indicates that tab is an NTP tab.
- (BOOL)isNTPTab:(Tab*)tab;

// Opens a tab at the specified URL and registers its JS-supplied window name if
// appropriate. For certain transition types, will consult the order controller
// and thus may only use |index| as a hint. |parentTab| may be nil if there
// is no parent associated with this new tab, as may |windowName| if not
// applicable. |openedByDOM| is YES if the page was opened by DOM.
// The |index| parameter can be set to
// TabModelConstants::kTabPositionAutomatically if the caller doesn't have a
// preference for the position of the tab.
- (Tab*)insertTabWithLoadParams:
            (const web::NavigationManager::WebLoadParams&)params
                     windowName:(NSString*)windowName
                         opener:(Tab*)parentTab
                    openedByDOM:(BOOL)openedByDOM
                        atIndex:(NSUInteger)index
                   inBackground:(BOOL)inBackground;

// Call to switch the selected tab. Broadcasts about the change in selection.
// It's ok for |newTab| to be nil in case the last tab is going away. In that
// case, the "tab deselected" notification gets sent, but no corresponding
// "tab selected" notification is sent. |persist| indicates whether or not
// the tab's state should be persisted in history upon switching.
- (void)changeSelectedTabFrom:(Tab*)oldTab
                           to:(Tab*)newTab
                 persistState:(BOOL)persist;

// Tells the snapshot cache the adjacent tab session ids.
- (void)updateSnapshotCache:(Tab*)tab;

// Helper method that posts a notification with the given name with |tab|
// in the userInfo dictionary under the kTabModelTabKey.
- (void)postNotificationName:(NSString*)notificationName withTab:(Tab*)tab;

// Helper method to restore a saved session and control if the state should
// be persisted or not. Used to implement the public -restoreSessionWindow:
// method and restoring session in the initialiser.
- (BOOL)restoreSessionWindow:(SessionWindowIOS*)window
                persistState:(BOOL)persistState;

@end

@implementation TabModel

@synthesize browserState = _browserState;
@synthesize sessionID = _sessionID;
@synthesize webUsageEnabled = webUsageEnabled_;

#pragma mark - Overriden

- (void)dealloc {
  DCHECK([_observers empty]);
  // browserStateDestroyed should always have been called before destruction.
  DCHECK(!_browserState);

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  // Make sure the tabs do clean after themselves. It is important for
  // removeObserver: to be called first otherwise a lot of unecessary work will
  // happen on -closeAllTabs.
  [self closeAllTabs];

  _clearPoliciesTaskTracker.TryCancelAll();

  [super dealloc];
}

#pragma mark - Public methods

- (Tab*)currentTab {
  return _currentTab.get();
}

- (void)setCurrentTab:(Tab*)newTab {
  DCHECK([_tabs containsObject:newTab]);
  if (_currentTab != newTab) {
    base::RecordAction(base::UserMetricsAction("MobileTabSwitched"));
    [self updateSnapshotCache:newTab];
  }
  if (_tabUsageRecorder) {
    _tabUsageRecorder->RecordTabSwitched(_currentTab, newTab);
  }
  [self changeSelectedTabFrom:_currentTab to:newTab persistState:YES];
}

- (TabModelSyncedWindowDelegate*)syncedWindowDelegate {
  return _syncedWindowDelegate.get();
}

- (TabUsageRecorder*)tabUsageRecorder {
  return _tabUsageRecorder.get();
}

- (BOOL)isOffTheRecord {
  return _browserState && _browserState->IsOffTheRecord();
}

- (BOOL)isEmpty {
  return self.count == 0;
}

- (NSUInteger)count {
  return [_tabs count];
}

- (instancetype)initWithSessionWindow:(SessionWindowIOS*)window
                       sessionService:(SessionServiceIOS*)service
                         browserState:(ios::ChromeBrowserState*)browserState {
  if ((self = [super init])) {
    _observers.reset([[TabModelObservers
        observersWithProtocol:@protocol(TabModelObserver)] retain]);

    _browserState = browserState;
    DCHECK(_browserState);

    // There must be a valid session service defined to consume session windows.
    DCHECK(service);
    _sessionService.reset([service retain]);

    // Normal browser states are the only ones to get tab restore. Tab sync
    // handles incognito browser states by filtering on profile, so it's
    // important to the backend code to always have a sync window delegate.
    if (!_browserState->IsOffTheRecord()) {
      // Set up the usage recorder before tabs are created.
      _tabUsageRecorder.reset(new TabUsageRecorder(self));
    }
    _syncedWindowDelegate.reset(new TabModelSyncedWindowDelegate(self));

    _tabs.reset([[NSMutableArray alloc] init]);
    if (window) {
      DCHECK([_observers empty]);
      // Restore the session and reset the session metrics (as the event have
      // not been generated by the user but by a cold start cycle).
      [self restoreSessionWindow:window persistState:NO];
      [self resetSessionMetrics];
    }

    _orderController.reset(
        [[TabModelOrderController alloc] initWithTabModel:self]);

    // Register for resign active notification.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(willResignActive:)
               name:UIApplicationWillResignActiveNotification
             object:nil];
    // Register for background notification.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationDidEnterBackground:)
               name:UIApplicationDidEnterBackgroundNotification
             object:nil];
    // Register for foregrounding notification.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationWillEnterForeground:)
               name:UIApplicationWillEnterForegroundNotification
             object:nil];

    // Associate with ios::ChromeBrowserState.
    RegisterTabModelWithChromeBrowserState(_browserState, self);
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (BOOL)restoreSessionWindow:(SessionWindowIOS*)window {
  return [self restoreSessionWindow:window persistState:YES];
}

- (void)saveSessionImmediately:(BOOL)immediately {
  // Do nothing if there are tabs in the model but no selected tab. This is
  // a transitional state.
  if ((!_currentTab && [_tabs count]) || !_browserState)
    return;
  [_sessionService saveWindow:self.windowForSavingSession
              forBrowserState:_browserState
                  immediately:immediately];
}

- (Tab*)tabAtIndex:(NSUInteger)index {
  return [_tabs objectAtIndex:index];
}

- (NSUInteger)indexOfTab:(Tab*)tab {
  return [_tabs indexOfObject:tab];
}

- (Tab*)tabWithWindowName:(NSString*)windowName {
  if (!windowName)
    return nil;
  for (Tab* tab in _tabs.get()) {
    if ([windowName isEqualToString:tab.windowName]) {
      return tab;
    }
  }
  return nil;
}

- (Tab*)nextTabWithOpener:(Tab*)tab afterTab:(Tab*)afterTab {
  NSUInteger startIndex = NSNotFound;
  // Start looking after |afterTab|. If it's not found, start looking after
  // |tab|. If it's not found either, bail.
  if (afterTab)
    startIndex = [self indexOfTab:afterTab];
  if (startIndex == NSNotFound)
    startIndex = [self indexOfTab:tab];
  if (startIndex == NSNotFound)
    return nil;
  NSString* parentID = tab.tabId;
  for (NSUInteger i = startIndex + 1; i < [_tabs count]; ++i) {
    Tab* current = [_tabs objectAtIndex:i];
    DCHECK([current navigationManager]);
    CRWSessionController* sessionController =
        [current navigationManager]->GetSessionController();
    if ([sessionController.openerId isEqualToString:parentID])
      return current;
  }
  return nil;
}

- (Tab*)firstTabWithOpener:(Tab*)tab {
  if (!tab)
    return nil;
  NSUInteger stopIndex = [self indexOfTab:tab];
  if (stopIndex == NSNotFound)
    return nil;
  NSString* parentID = tab.tabId;
  // Match the navigation index as well as the session id, to better match the
  // state of the tab. I.e. two tabs are opened via a link from tab A, and then
  // a new url is loaded into tab A, and more tabs opened from that url, the
  // latter two tabs should not be grouped with the former two. The navigation
  // index is the simplest way to detect navigation changes.
  DCHECK([tab navigationManager]);
  NSInteger parentNavIndex = [tab navigationManager]->GetCurrentItemIndex();
  for (NSUInteger i = 0; i < stopIndex; ++i) {
    Tab* tabToCheck = [_tabs objectAtIndex:i];
    DCHECK([tabToCheck navigationManager]);
    CRWSessionController* sessionController =
        [tabToCheck navigationManager]->GetSessionController();
    if ([sessionController.openerId isEqualToString:parentID] &&
        sessionController.openerNavigationIndex == parentNavIndex) {
      return tabToCheck;
    }
  }
  return nil;
}

- (Tab*)lastTabWithOpener:(Tab*)tab {
  NSUInteger startIndex = [self indexOfTab:tab];
  if (startIndex == NSNotFound)
    return nil;
  // There is at least one tab in the model, because otherwise the above check
  // would have returned.
  NSString* parentID = tab.tabId;
  DCHECK([tab navigationManager]);
  NSInteger parentNavIndex = [tab navigationManager]->GetCurrentItemIndex();

  Tab* match = nil;
  // Find the last tab in the first matching 'group'.  A 'group' is a set of
  // tabs whose opener's id and opener's navigation index match. The navigation
  // index is used in addition to the session id to detect navigations changes
  // within the same session.
  for (NSUInteger i = startIndex + 1; i < [_tabs count]; ++i) {
    Tab* tabToCheck = [_tabs objectAtIndex:i];
    DCHECK([tabToCheck navigationManager]);
    CRWSessionController* sessionController =
        [tabToCheck navigationManager]->GetSessionController();
    if ([sessionController.openerId isEqualToString:parentID] &&
        sessionController.openerNavigationIndex == parentNavIndex) {
      match = tabToCheck;
    } else if (match) {
      break;
    }
  }
  return match;
}

- (Tab*)openerOfTab:(Tab*)tab {
  if (![tab navigationManager])
    return nil;
  NSString* openerId = [tab navigationManager]->GetSessionController().openerId;
  if (!openerId.length)  // Short-circuit if opener is empty.
    return nil;
  for (Tab* iteratedTab in _tabs.get()) {
    if ([iteratedTab.tabId isEqualToString:openerId])
      return iteratedTab;
  }
  return nil;
}

- (Tab*)insertOrUpdateTabWithURL:(const GURL&)URL
                        referrer:(const web::Referrer&)referrer
                      transition:(ui::PageTransition)transition
                      windowName:(NSString*)windowName
                          opener:(Tab*)parentTab
                     openedByDOM:(BOOL)openedByDOM
                         atIndex:(NSUInteger)index
                    inBackground:(BOOL)inBackground {
  web::NavigationManager::WebLoadParams params(URL);
  params.referrer = referrer;
  params.transition_type = transition;
  return [self insertOrUpdateTabWithLoadParams:params
                                    windowName:windowName
                                        opener:parentTab
                                   openedByDOM:openedByDOM
                                       atIndex:index
                                  inBackground:inBackground];
}

- (Tab*)insertOrUpdateTabWithLoadParams:
            (const web::NavigationManager::WebLoadParams&)loadParams
                             windowName:(NSString*)windowName
                                 opener:(Tab*)parentTab
                            openedByDOM:(BOOL)openedByDOM
                                atIndex:(NSUInteger)index
                           inBackground:(BOOL)inBackground {
  // Find the tab for the given window name. If found, load with
  // |originalParams| in it, otherwise create a new tab for it.
  Tab* tab = [self tabWithWindowName:windowName];
  if (tab) {
    // Updating a tab shouldn't be possible with web usage suspended, since
    // whatever page would be driving it should also be suspended.
    DCHECK(webUsageEnabled_);

    web::NavigationManager::WebLoadParams updatedParams(loadParams);
    updatedParams.is_renderer_initiated = (parentTab != nil);
    [tab.webController loadWithParams:updatedParams];

    // Force the page to start loading even if it's in the background.
    [tab.webController triggerPendingLoad];

    if (!inBackground)
      [self setCurrentTab:tab];
  } else {
    tab = [self insertTabWithLoadParams:loadParams
                             windowName:windowName
                                 opener:parentTab
                            openedByDOM:openedByDOM
                                atIndex:index
                           inBackground:inBackground];
  }

  return tab;
}

- (Tab*)insertBlankTabWithTransition:(ui::PageTransition)transition
                              opener:(Tab*)parentTab
                         openedByDOM:(BOOL)openedByDOM
                             atIndex:(NSUInteger)index
                        inBackground:(BOOL)inBackground {
  GURL emptyURL;
  web::NavigationManager::WebLoadParams params(emptyURL);
  params.transition_type = transition;
  // Tabs open by DOM are always renderer initiated.
  params.is_renderer_initiated = openedByDOM;
  return [self insertTabWithLoadParams:params
                            windowName:nil
                                opener:parentTab
                           openedByDOM:openedByDOM
                               atIndex:index
                          inBackground:inBackground];
}

- (Tab*)insertTabWithWebState:(std::unique_ptr<web::WebState>)webState
                      atIndex:(NSUInteger)index {
  DCHECK(_browserState);
  DCHECK_EQ(webState->GetBrowserState(), _browserState);
  base::scoped_nsobject<Tab> tab(
      [[Tab alloc] initWithWebState:std::move(webState) model:self]);
  [tab webController].webUsageEnabled = webUsageEnabled_;
  [self insertTab:tab atIndex:index];
  return tab;
}

- (void)insertTab:(Tab*)tab atIndex:(NSUInteger)index {
  DCHECK(tab);
  DCHECK(index <= [_tabs count]);
  [tab fetchFavicon];
  [_tabs insertObject:tab atIndex:index];

  TabParentingGlobalObserver::GetInstance()->OnTabParented(tab.webState);
  [_observers tabModel:self didInsertTab:tab atIndex:index inForeground:NO];
  [_observers tabModelDidChangeTabCount:self];

  base::RecordAction(base::UserMetricsAction("MobileNewTabOpened"));
  // Persist the session due to a new tab being inserted. If this is a
  // background tab (will not become active), saving now will capture the
  // state properly. If it does eventually become active, another save will
  // be triggered to properly capture the end result.
  [self saveSessionImmediately:NO];
  ++_newTabCount;
}

- (void)moveTab:(Tab*)tab toIndex:(NSUInteger)toIndex {
  NSUInteger fromIndex = [self indexOfTab:tab];
  DCHECK_NE(NSNotFound, static_cast<NSInteger>(fromIndex));
  DCHECK_LT(toIndex, self.count);
  if (fromIndex == NSNotFound || toIndex >= self.count ||
      fromIndex == toIndex) {
    return;
  }

  base::scoped_nsobject<Tab> tabSaver([tab retain]);
  [_tabs removeObject:tab];
  [_tabs insertObject:tab atIndex:toIndex];

  [_observers tabModel:self didMoveTab:tab fromIndex:fromIndex toIndex:toIndex];
}

- (void)replaceTab:(Tab*)oldTab withTab:(Tab*)newTab {
  NSUInteger index = [self indexOfTab:oldTab];
  DCHECK_NE(NSNotFound, static_cast<NSInteger>(index));

  base::scoped_nsobject<Tab> tabSaver([oldTab retain]);
  [newTab fetchFavicon];
  [_tabs replaceObjectAtIndex:index withObject:newTab];
  [newTab setParentTabModel:self];

  TabParentingGlobalObserver::GetInstance()->OnTabParented(newTab.webState);
  [_observers tabModel:self didReplaceTab:oldTab withTab:newTab atIndex:index];

  if (self.currentTab == oldTab)
    [self changeSelectedTabFrom:nil to:newTab persistState:NO];

  [oldTab setParentTabModel:nil];
  [oldTab close];

  // Record a tab clobber, since swapping tabs bypasses the tab code that would
  // normally log clobbers.
  base::RecordAction(base::UserMetricsAction("MobileTabClobbered"));
}

- (void)closeTabAtIndex:(NSUInteger)index {
  DCHECK(index < [_tabs count]);
  [self closeTab:[_tabs objectAtIndex:index]];
}

- (void)closeTab:(Tab*)tab {
  // Ensure the tab stays alive long enough for us to send out the
  // notice of its destruction to the delegate.
  [_observers tabModel:self willRemoveTab:tab];
  [tab close];  // Note it is not safe to access the tab after 'close'.
}

- (void)closeAllTabs {
  // If this changes, _closedTabCount metrics need to be adjusted.
  for (NSInteger i = self.count - 1; i >= 0; --i)
    [self closeTabAtIndex:i];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kTabModelAllTabsDidCloseNotification
                    object:self];
}

- (void)haltAllTabs {
  for (Tab* tab in _tabs.get()) {
    [tab terminateNetworkActivity];
  }
}

- (void)notifyTabChanged:(Tab*)tab {
  [_observers tabModel:self didChangeTab:tab];
}

- (void)addObserver:(id<TabModelObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<TabModelObserver>)observer {
  [_observers removeObserver:observer];
}

- (void)resetSessionMetrics {
  _closedTabCount = 0;
  _openedTabCount = 0;
  _newTabCount = 0;
}

- (void)recordSessionMetrics {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Session.ClosedTabCounts", _closedTabCount, 1,
                              200, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Session.OpenedTabCounts", _openedTabCount, 1,
                              200, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Session.NewTabCounts", _newTabCount, 1, 200, 50);
}

- (void)notifyTabSnapshotChanged:(Tab*)tab withImage:(UIImage*)image {
  DCHECK([NSThread isMainThread]);
  [_observers tabModel:self didChangeTabSnapshot:tab withImage:image];
}

- (void)resetAllWebViews {
  for (Tab* tab in _tabs.get()) {
    [tab.webController reinitializeWebViewAndReload:(tab == _currentTab)];
  }
}

- (void)setWebUsageEnabled:(BOOL)webUsageEnabled {
  if (webUsageEnabled_ == webUsageEnabled)
    return;
  webUsageEnabled_ = webUsageEnabled;
  for (Tab* tab in _tabs.get()) {
    tab.webUsageEnabled = webUsageEnabled;
  }
}

- (void)setPrimary:(BOOL)primary {
  if (_tabUsageRecorder)
    _tabUsageRecorder->RecordPrimaryTabModelChange(primary, _currentTab);
}

- (NSSet*)currentlyReferencedExternalFiles {
  NSMutableSet* referencedFiles = [NSMutableSet set];
  if (!_browserState)
    return referencedFiles;
  // Check the currently open tabs for external files.
  for (Tab* tab in _tabs.get()) {
    if (UrlIsExternalFileReference(tab.url)) {
      NSString* fileName = base::SysUTF8ToNSString(tab.url.ExtractFileName());
      [referencedFiles addObject:fileName];
    }
  }
  // Do the same for the recently closed tabs.
  sessions::TabRestoreService* restoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(_browserState);
  DCHECK(restoreService);
  for (const auto& entry : restoreService->entries()) {
    sessions::TabRestoreService::Tab* tab =
        static_cast<sessions::TabRestoreService::Tab*>(entry.get());
    int navigationIndex = tab->current_navigation_index;
    sessions::SerializedNavigationEntry navigation =
        tab->navigations[navigationIndex];
    GURL URL = navigation.virtual_url();
    if (UrlIsExternalFileReference(URL)) {
      NSString* fileName = base::SysUTF8ToNSString(URL.ExtractFileName());
      [referencedFiles addObject:fileName];
    }
  }
  return referencedFiles;
}

// NOTE: This can be called multiple times, so must be robust against that.
- (void)browserStateDestroyed {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  if (_browserState) {
    UnregisterTabModelFromChromeBrowserState(_browserState, self);
  }
  _browserState = nullptr;
}

// Called when a tab is closing, but before its CRWWebController is destroyed.
// Equivalent to DetachTabContentsAt() in Chrome's TabStripModel.
- (void)didCloseTab:(Tab*)closedTab {
  NSUInteger closedTabIndex = [_tabs indexOfObject:closedTab];
  DCHECK(closedTab);
  DCHECK(closedTabIndex != NSNotFound);
  // Let the sessions::TabRestoreService know about that new tab.
  sessions::TabRestoreService* restoreService =
      _browserState
          ? IOSChromeTabRestoreServiceFactory::GetForBrowserState(_browserState)
          : nullptr;
  web::NavigationManagerImpl* navigationManager = [closedTab navigationManager];
  DCHECK(navigationManager);
  int itemCount = navigationManager->GetItemCount();
  if (restoreService && (![self isNTPTab:closedTab] || itemCount > 1)) {
    restoreService->CreateHistoricalTab(
        sessions::IOSLiveTab::GetForWebState(closedTab.webState),
        static_cast<int>(closedTabIndex));
  }
  // This needs to be called before the tab is removed from the list.
  Tab* newSelection =
      [_orderController determineNewSelectedTabFromRemovedTab:closedTab];
  base::scoped_nsobject<Tab> kungFuDeathGrip([closedTab retain]);
  [_tabs removeObject:closedTab];

  // If closing the current tab, clear |_currentTab| before sending any
  // notification. This avoids various parts of the code getting confused
  // when the current tab isn't in the tab model.
  Tab* savedCurrentTab = _currentTab;
  if (closedTab == _currentTab)
    _currentTab.reset(nil);

  [_observers tabModel:self didRemoveTab:closedTab atIndex:closedTabIndex];
  [_observers tabModelDidChangeTabCount:self];

  // Current tab has closed, update the selected tab and swap in its
  // contents. There is nothing to do if a non-selected tab is closed as
  // the selection isn't index-based, therefore it hasn't changed.
  // -changeSelectedTabFrom: will persist the state change, so only do it
  // if the selection isn't changing.
  if (closedTab == savedCurrentTab) {
    [self changeSelectedTabFrom:closedTab to:newSelection persistState:NO];
  } else {
    [self saveSessionImmediately:NO];
  }
  base::RecordAction(base::UserMetricsAction("MobileTabClosed"));
  ++_closedTabCount;
}

- (void)navigationCommittedInTab:(Tab*)tab {
  if (self.offTheRecord)
    return;
  if (![tab navigationManager])
    return;

  // See if the navigation was within a page; if so ignore it.
  web::NavigationItem* previousItem =
      [tab navigationManager]->GetPreviousItem();
  if (previousItem) {
    GURL previousURL = previousItem->GetURL();
    GURL currentURL = [tab navigationManager]->GetVisibleItem()->GetURL();

    url::Replacements<char> replacements;
    replacements.ClearRef();
    if (previousURL.ReplaceComponents(replacements) ==
        currentURL.ReplaceComponents(replacements)) {
      return;
    }
  }

  int tabCount = static_cast<int>(self.count);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerLoad", tabCount, 1, 200, 50);
}

#pragma mark - NSFastEnumeration

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState*)state
                                  objects:(id*)objects
                                    count:(NSUInteger)count {
  return [_tabs countByEnumeratingWithState:state objects:objects count:count];
}

#pragma mark - TabUsageRecorderDelegate

- (NSUInteger)liveTabsCount {
  NSUInteger count = 0;
  NSArray* tabs = _tabs.get();
  for (Tab* tab in tabs) {
    if ([tab.webController isViewAlive])
      count++;
  }
  return count;
}

#pragma mark - Private methods

- (SessionWindowIOS*)windowForSavingSession {
  // Background tabs will already have their state preserved, but not the
  // fg tab. Do it now.
  [_currentTab recordStateInHistory];

  // Build the array of sessions. Copy the session objects as the saving will
  // be done on a separate thread.
  // TODO(crbug.com/661986): This could get expensive especially since this
  // window may never be saved (if another call comes in before the delay).
  SessionWindowIOS* window = [[[SessionWindowIOS alloc] init] autorelease];
  for (Tab* tab in self) {
    web::WebState* webState = tab.webState;
    DCHECK(webState);
    [window addSerializedSession:webState->BuildSerializedNavigationManager()];
  }
  window.selectedIndex = [self indexOfTab:_currentTab];
  return window;
}

- (BOOL)isNTPTab:(Tab*)tab {
  std::string host = tab.url.host();
  return host == kChromeUINewTabHost || host == kChromeUIBookmarksHost;
}

- (Tab*)insertTabWithLoadParams:
            (const web::NavigationManager::WebLoadParams&)params
                     windowName:(NSString*)windowName
                         opener:(Tab*)parentTab
                    openedByDOM:(BOOL)openedByDOM
                        atIndex:(NSUInteger)index
                   inBackground:(BOOL)inBackground {
  DCHECK(_browserState);
  base::scoped_nsobject<Tab> tab([[Tab alloc]
      initWithWindowName:windowName
                  opener:parentTab
             openedByDOM:openedByDOM
                   model:self
            browserState:_browserState]);
  [tab webController].webUsageEnabled = webUsageEnabled_;

  if ((PageTransitionCoreTypeIs(params.transition_type,
                                ui::PAGE_TRANSITION_LINK)) &&
      (index == TabModelConstants::kTabPositionAutomatically)) {
    DCHECK(!parentTab || [self indexOfTab:parentTab] != NSNotFound);
    // Assume tabs opened via link clicks are part of the same "task" as their
    // parent and are grouped together.
    TabModelOrderConstants::InsertionAdjacency adjacency =
        inBackground ? TabModelOrderConstants::kAdjacentAfter
                     : TabModelOrderConstants::kAdjacentBefore;
    index = [_orderController insertionIndexForTab:tab
                                        transition:params.transition_type
                                            opener:parentTab
                                         adjacency:adjacency];
  } else {
    // For all other types, respect what was passed to us, normalizing values
    // that are too large.
    if (index >= self.count)
      index = [_orderController insertionIndexForAppending];
  }

  if (PageTransitionCoreTypeIs(params.transition_type,
                               ui::PAGE_TRANSITION_TYPED) &&
      index == self.count) {
    // Also, any tab opened at the end of the TabStrip with a "TYPED"
    // transition inherit group as well. This covers the cases where the user
    // creates a New Tab (e.g. Ctrl+T, or clicks the New Tab button), or types
    // in the address bar and presses Alt+Enter. This allows for opening a new
    // Tab to quickly look up something. When this Tab is closed, the old one
    // is re-selected, not the next-adjacent.
    // TODO(crbug.com/661988): Make this work.
  }

  [self insertTab:tab atIndex:index];

  if (!inBackground && _tabUsageRecorder)
    _tabUsageRecorder->TabCreatedForSelection(tab);

  [[tab webController] loadWithParams:params];
  // Force the page to start loading even if it's in the background.
  if (webUsageEnabled_)
    [[tab webController] triggerPendingLoad];
  NSDictionary* userInfo = @{
    kTabModelTabKey : tab,
    kTabModelOpenInBackgroundKey : @(inBackground),
  };
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kTabModelNewTabWillOpenNotification
                    object:self
                  userInfo:userInfo];

  if (!inBackground)
    [self setCurrentTab:tab];

  return tab;
}

- (void)changeSelectedTabFrom:(Tab*)oldTab
                           to:(Tab*)newTab
                 persistState:(BOOL)persist {
  if (oldTab) {
    // Save state, such as scroll position, before switching tabs.
    if (oldTab != newTab && persist)
      [oldTab recordStateInHistory];
    [self postNotificationName:kTabModelTabDeselectedNotification
                       withTab:oldTab];
  }

  // No Tab to select (e.g. the last Tab has been closed).
  if ([self indexOfTab:newTab] == NSNotFound)
    return;

  _currentTab.reset(newTab);
  if (newTab) {
    [_observers tabModel:self
        didChangeActiveTab:newTab
               previousTab:oldTab
                   atIndex:[self indexOfTab:newTab]];
    [newTab updateLastVisitedTimestamp];
    ++_openedTabCount;
  }
  BOOL loadingFinished = [newTab.webController loadPhase] == web::PAGE_LOADED;
  if (loadingFinished) {
    // Persist the session state.
    [self saveSessionImmediately:NO];
  }
}

- (void)updateSnapshotCache:(Tab*)tab {
  NSMutableSet* set = [NSMutableSet set];
  NSUInteger index = [self indexOfTab:tab];
  if (index > 0) {
    Tab* previousTab = [self tabAtIndex:(index - 1)];
    [set addObject:previousTab.tabId];
  }
  if (index < self.count - 1) {
    Tab* nextTab = [self tabAtIndex:(index + 1)];
    [set addObject:nextTab.tabId];
  }
  [SnapshotCache sharedInstance].pinnedIDs = set;
}

- (void)postNotificationName:(NSString*)notificationName withTab:(Tab*)tab {
  // A scoped_nsobject is used rather than an NSDictionary with static
  // initializer dictionaryWithObject, because that approach adds the dictionary
  // to the autorelease pool, which in turn holds Tab alive longer than
  // necessary.
  base::scoped_nsobject<NSDictionary> userInfo(
      [[NSDictionary alloc] initWithObjectsAndKeys:tab, kTabModelTabKey, nil]);
  [[NSNotificationCenter defaultCenter] postNotificationName:notificationName
                                                      object:self
                                                    userInfo:userInfo];
}

- (BOOL)restoreSessionWindow:(SessionWindowIOS*)window
                persistState:(BOOL)persistState {
  DCHECK(_browserState);
  DCHECK(window);

  NSArray* sessions = window.sessions;
  if (!sessions.count)
    return NO;

  size_t oldCount = [_tabs count];
  web::WebState::CreateParams params(_browserState);
  scoped_refptr<web::CertificatePolicyCache> policyCache =
      web::BrowserState::GetCertificatePolicyCache(_browserState);

  for (CRWNavigationManagerStorage* session in sessions) {
    std::unique_ptr<web::WebState> webState =
        web::WebState::Create(params, session);
    DCHECK_EQ(webState->GetBrowserState(), _browserState);
    Tab* tab =
        [self insertTabWithWebState:std::move(webState) atIndex:[_tabs count]];
    tab.webController.usePlaceholderOverlay = YES;

    // Restore the CertificatePolicyCache (note that webState is invalid after
    // passing it via move semantic to -insertTabWithWebState:atIndex:).
    UpdateCertificatePolicyCacheFromWebState(policyCache, tab.webState);
  }
  DCHECK_GT([_tabs count], oldCount);

  // Update the selected tab if there was a selected Tab in the saved session.
  if (window.selectedIndex != NSNotFound) {
    NSUInteger selectedIndex = window.selectedIndex + oldCount;
    DCHECK_LT(selectedIndex, [_tabs count]);
    DCHECK([self tabAtIndex:selectedIndex]);
    [self changeSelectedTabFrom:_currentTab
                             to:[self tabAtIndex:selectedIndex]
                   persistState:persistState];
  }

  // If there was only one tab and it was the new tab page, clobber it.
  BOOL closedNTPTab = NO;
  if (oldCount == 1) {
    Tab* tab = [_tabs objectAtIndex:0];
    if (tab.url == GURL(kChromeUINewTabURL)) {
      [self closeTab:tab];
      closedNTPTab = YES;
      oldCount = 0;
    }
  }
  if (_tabUsageRecorder) {
    _tabUsageRecorder->InitialRestoredTabs(
        _currentTab,
        [_tabs
            subarrayWithRange:NSMakeRange(oldCount, [_tabs count] - oldCount)]);
  }
  return closedNTPTab;
}

#pragma mark - Notification Handlers

// Called when UIApplicationWillResignActiveNotification is received.
- (void)willResignActive:(NSNotification*)notify {
  if (webUsageEnabled_ && _currentTab) {
    [[SnapshotCache sharedInstance]
        willBeSavedGreyWhenBackgrounding:_currentTab.get().tabId];
  }
}

// Called when UIApplicationDidEnterBackgroundNotification is received.
- (void)applicationDidEnterBackground:(NSNotification*)notify {
  if (!_browserState)
    return;

  // Evict all the certificate policies except for the current entries of the
  // active sessions.
  CleanCertificatePolicyCache(
      &_clearPoliciesTaskTracker,
      web::WebThread::GetTaskRunnerForThread(web::WebThread::IO),
      web::BrowserState::GetCertificatePolicyCache(_browserState), self);

  if (_tabUsageRecorder)
    _tabUsageRecorder->AppDidEnterBackground();

  // Normally, the session is saved after some timer expires but since the app
  // is about to enter the background send YES to save the session immediately.
  [self saveSessionImmediately:YES];

  // Write out a grey version of the current website to disk.
  if (webUsageEnabled_ && _currentTab) {
    [[SnapshotCache sharedInstance]
        saveGreyInBackgroundForSessionID:_currentTab.get().tabId];
  }
}

// Called when UIApplicationWillEnterForegroundNotification is received.
- (void)applicationWillEnterForeground:(NSNotification*)notify {
  if (_tabUsageRecorder) {
    _tabUsageRecorder->AppWillEnterForeground();
  }
}

@end

@implementation TabModel (PrivateForTestingOnly)

- (Tab*)addTabWithURL:(const GURL&)URL
             referrer:(const web::Referrer&)referrer
           windowName:(NSString*)windowName {
  return [self insertTabWithURL:URL
                       referrer:referrer
                     windowName:windowName
                         opener:nil
                        atIndex:[_orderController insertionIndexForAppending]];
}

- (Tab*)insertTabWithURL:(const GURL&)URL
                referrer:(const web::Referrer&)referrer
              windowName:(NSString*)windowName
                  opener:(Tab*)parentTab
                 atIndex:(NSUInteger)index {
  DCHECK(_browserState);
  base::scoped_nsobject<Tab> tab([[Tab alloc]
      initWithWindowName:windowName
                  opener:parentTab
             openedByDOM:NO
                   model:self
            browserState:_browserState]);
  web::NavigationManager::WebLoadParams params(URL);
  params.referrer = referrer;
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  [[tab webController] loadWithParams:params];
  [tab webController].webUsageEnabled = webUsageEnabled_;
  [self insertTab:tab atIndex:index];
  return tab;
}

@end
