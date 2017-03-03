// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/bind.h"
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
#import "ios/chrome/browser/metrics/tab_usage_recorder_web_state_list_observer.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_service.h"
#import "ios/chrome/browser/sessions/session_window.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_web_state_list_observer.h"
#include "ios/chrome/browser/tab_parenting_global_observer.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#import "ios/chrome/browser/tabs/tab_model_observers.h"
#import "ios/chrome/browser/tabs/tab_model_observers_bridge.h"
#import "ios/chrome/browser/tabs/tab_model_selected_tab_observer.h"
#import "ios/chrome/browser/tabs/tab_model_synced_window_delegate.h"
#import "ios/chrome/browser/tabs/tab_parenting_observer.h"
#import "ios/chrome/browser/xcallback_parameters.h"
#import "ios/shared/chrome/browser/tabs/web_state_list.h"
#import "ios/shared/chrome/browser/tabs/web_state_list_fast_enumeration_helper.h"
#import "ios/shared/chrome/browser/tabs/web_state_list_metrics_observer.h"
#import "ios/shared/chrome/browser/tabs/web_state_list_observer.h"
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

// Populates the certificate policy cache based on the WebStates of
// |web_state_list|.
void RestoreCertificatePolicyCacheFromModel(
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    WebStateList* web_state_list) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  for (int index = 0; index < web_state_list->count(); ++index) {
    UpdateCertificatePolicyCacheFromWebState(
        policy_cache, web_state_list->GetWebStateAt(index));
  }
}

// Scrubs the certificate policy cache of all certificates policies except
// those for the current entries in |web_state_list|.
void CleanCertificatePolicyCache(
    base::CancelableTaskTracker* task_tracker,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    WebStateList* web_state_list) {
  DCHECK(policy_cache);
  DCHECK(web_state_list);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  task_tracker->PostTaskAndReply(
      task_runner.get(), FROM_HERE,
      base::Bind(&web::CertificatePolicyCache::ClearCertificatePolicies,
                 policy_cache),
      base::Bind(&RestoreCertificatePolicyCacheFromModel, policy_cache,
                 base::Unretained(web_state_list)));
}

// Internal helper function returning the opener for a given Tab by
// checking the associated Tab tabId (should be removed once the opener
// is passed to the insertTab:atIndex: and replaceTab:withTab: methods).
Tab* GetOpenerForTab(id<NSFastEnumeration> tabs, Tab* tab) {
  if (!tab.openerID)
    return nullptr;

  for (Tab* currentTab in tabs) {
    if ([tab.openerID isEqualToString:currentTab.tabId])
      return currentTab;
  }

  return nullptr;
}

}  // anonymous namespace

@interface TabModelWebStateProxyFactory : NSObject<WebStateProxyFactory>
@end

@implementation TabModelWebStateProxyFactory

- (id)proxyForWebState:(web::WebState*)webState {
  return LegacyTabHelper::GetTabForWebState(webState);
}

@end

@interface TabModel ()<TabUsageRecorderDelegate> {
  // Underlying shared model implementation.
  WebStateList _webStateList;

  // Helper providing NSFastEnumeration implementation over the WebStateList.
  base::scoped_nsobject<WebStateListFastEnumerationHelper>
      _fastEnumerationHelper;

  // Used to keep the Tabs alive while the corresponding WebStates are stored
  // in the WebStateList (as Tabs currently own their WebState). Remove once
  // WebState owns the associated Tab.
  base::scoped_nsobject<NSMutableSet<Tab*>> _tabRetainer;

  // WebStateListObserver bridges to react to modifications of the model (may
  // send notification, translate and forward events, update metrics, ...).
  std::vector<std::unique_ptr<WebStateListObserver>> _observerBridges;

  // The delegate for sync.
  std::unique_ptr<TabModelSyncedWindowDelegate> _syncedWindowDelegate;

  // Counters for metrics.
  WebStateListMetricsObserver* _webStateListMetricsObserver;

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

// Helper method that posts a notification with the given name with |tab|
// in the userInfo dictionary under the kTabModelTabKey.
- (void)postNotificationName:(NSString*)notificationName withTab:(Tab*)tab;

// Helper method to restore a saved session and control if the state should
// be persisted or not. Used to implement the public -restoreSessionWindow:
// method and restoring session in the initialiser.
- (BOOL)restoreSessionWindow:(SessionWindowIOS*)window
                persistState:(BOOL)persistState;

// Helper method to insert |tab| at the given |index| recording |parentTab| as
// the opener. Broadcasts the proper notifications about the change. The
// receiver should be set as the parentTabModel for |tab|; this method doesn't
// check that.
- (void)insertTab:(Tab*)tab atIndex:(NSUInteger)index opener:(Tab*)parentTab;

// Helper method to insert |tab| at the given |index| recording |parentTab| as
// the opener. Broadcasts the proper notifications about the change. The
// receiver should be set as the parentTabModel for |tab|; this method doesn't
// check that.
- (void)insertTab:(Tab*)tab
          atIndex:(NSUInteger)index
           opener:(Tab*)parentTab
       transition:(ui::PageTransition)transition;

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

  // Clear weak pointer to WebStateListMetricsObserver before destroying it.
  _webStateListMetricsObserver = nullptr;

  // Unregister all listeners before closing all the tabs.
  for (const auto& observerBridge : _observerBridges)
    _webStateList.RemoveObserver(observerBridge.get());
  _observerBridges.clear();

  // Make sure the tabs do clean after themselves. It is important for
  // removeObserver: to be called first otherwise a lot of unecessary work will
  // happen on -closeAllTabs.
  [self closeAllTabs];

  _clearPoliciesTaskTracker.TryCancelAll();

  [super dealloc];
}

#pragma mark - Public methods

- (Tab*)currentTab {
  web::WebState* webState = _webStateList.GetActiveWebState();
  return webState ? LegacyTabHelper::GetTabForWebState(webState) : nil;
}

- (void)setCurrentTab:(Tab*)newTab {
  int indexOfTab = _webStateList.GetIndexOfWebState(newTab.webState);
  DCHECK_NE(indexOfTab, WebStateList::kInvalidIndex);
  _webStateList.ActivateWebStateAt(indexOfTab);
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
  return _webStateList.empty();
}

- (NSUInteger)count {
  DCHECK_GE(_webStateList.count(), 0);
  return static_cast<NSUInteger>(_webStateList.count());
}

- (instancetype)initWithSessionWindow:(SessionWindowIOS*)window
                       sessionService:(SessionServiceIOS*)service
                         browserState:(ios::ChromeBrowserState*)browserState {
  if ((self = [super init])) {
    _tabRetainer.reset([[NSMutableSet alloc] init]);
    _observers.reset([[TabModelObservers observers] retain]);

    _fastEnumerationHelper.reset([[WebStateListFastEnumerationHelper alloc]
        initWithWebStateList:&_webStateList
                proxyFactory:[[TabModelWebStateProxyFactory alloc] init]]);

    _browserState = browserState;
    DCHECK(_browserState);

    // Normal browser states are the only ones to get tab restore. Tab sync
    // handles incognito browser states by filtering on profile, so it's
    // important to the backend code to always have a sync window delegate.
    if (!_browserState->IsOffTheRecord()) {
      // Set up the usage recorder before tabs are created.
      _tabUsageRecorder = base::MakeUnique<TabUsageRecorder>(self);
    }
    _syncedWindowDelegate =
        base::MakeUnique<TabModelSyncedWindowDelegate>(self);

    // There must be a valid session service defined to consume session windows.
    DCHECK(service);
    _sessionService.reset([service retain]);

    _observerBridges.push_back(
        base::MakeUnique<SnapshotCacheWebStateListObserver>(
            [SnapshotCache sharedInstance]));
    if (_tabUsageRecorder) {
      _observerBridges.push_back(
          base::MakeUnique<TabUsageRecorderWebStateListObserver>(
              _tabUsageRecorder.get()));
    }
    _observerBridges.push_back(base::MakeUnique<TabParentingObserver>());
    _observerBridges.push_back(base::MakeUnique<WebStateListObserverBridge>(
        [[TabModelSelectedTabObserver alloc] initWithTabModel:self]));
    _observerBridges.push_back(base::MakeUnique<WebStateListObserverBridge>(
        [[TabModelObserversBridge alloc] initWithTabModel:self
                                        tabModelObservers:_observers.get()]));

    auto webStateListMetricsObserver =
        base::MakeUnique<WebStateListMetricsObserver>();
    _webStateListMetricsObserver = webStateListMetricsObserver.get();
    _observerBridges.push_back(std::move(webStateListMetricsObserver));

    for (const auto& observerBridge : _observerBridges)
      _webStateList.AddObserver(observerBridge.get());

    if (window) {
      DCHECK([_observers empty]);
      // Restore the session and reset the session metrics (as the event have
      // not been generated by the user but by a cold start cycle).
      [self restoreSessionWindow:window persistState:NO];
      [self resetSessionMetrics];
    }

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
  if ((!self.currentTab && _webStateList.count()) || !_browserState)
    return;
  [_sessionService saveWindow:self.windowForSavingSession
              forBrowserState:_browserState
                  immediately:immediately];
}

- (Tab*)tabAtIndex:(NSUInteger)index {
  DCHECK_LE(index, static_cast<NSUInteger>(INT_MAX));
  return LegacyTabHelper::GetTabForWebState(
      _webStateList.GetWebStateAt(static_cast<int>(index)));
}

- (NSUInteger)indexOfTab:(Tab*)tab {
  int index = _webStateList.GetIndexOfWebState(tab.webState);
  if (index == WebStateList::kInvalidIndex)
    return NSNotFound;

  DCHECK_GE(index, 0);
  return static_cast<NSUInteger>(index);
}

- (Tab*)nextTabWithOpener:(Tab*)tab afterTab:(Tab*)afterTab {
  int startIndex = WebStateList::kInvalidIndex;
  if (afterTab)
    startIndex = _webStateList.GetIndexOfWebState(afterTab.webState);

  if (startIndex == WebStateList::kInvalidIndex)
    startIndex = _webStateList.GetIndexOfWebState(tab.webState);

  const int index = _webStateList.GetIndexOfNextWebStateOpenedBy(
      tab.webState, startIndex, false);
  if (index == WebStateList::kInvalidIndex)
    return nil;

  DCHECK_GE(index, 0);
  return [self tabAtIndex:static_cast<NSUInteger>(index)];
}

- (Tab*)lastTabWithOpener:(Tab*)tab {
  int startIndex = _webStateList.GetIndexOfWebState(tab.webState);
  if (startIndex == WebStateList::kInvalidIndex)
    return nil;

  const int index = _webStateList.GetIndexOfLastWebStateOpenedBy(
      tab.webState, startIndex, true);
  if (index == WebStateList::kInvalidIndex)
    return nil;

  DCHECK_GE(index, 0);
  return [self tabAtIndex:static_cast<NSUInteger>(index)];
}

- (Tab*)openerOfTab:(Tab*)tab {
  int index = _webStateList.GetIndexOfWebState(tab.webState);
  if (index == WebStateList::kInvalidIndex)
    return nil;

  web::WebState* opener = _webStateList.GetOpenerOfWebStateAt(index);
  return opener ? LegacyTabHelper::GetTabForWebState(opener) : nil;
}

- (Tab*)insertTabWithURL:(const GURL&)URL
                referrer:(const web::Referrer&)referrer
              transition:(ui::PageTransition)transition
                  opener:(Tab*)parentTab
             openedByDOM:(BOOL)openedByDOM
                 atIndex:(NSUInteger)index
            inBackground:(BOOL)inBackground {
  web::NavigationManager::WebLoadParams params(URL);
  params.referrer = referrer;
  params.transition_type = transition;
  return [self insertTabWithLoadParams:params
                                opener:parentTab
                           openedByDOM:openedByDOM
                               atIndex:index
                          inBackground:inBackground];
}

- (Tab*)insertTabWithLoadParams:
            (const web::NavigationManager::WebLoadParams&)params
                         opener:(Tab*)parentTab
                    openedByDOM:(BOOL)openedByDOM
                        atIndex:(NSUInteger)index
                   inBackground:(BOOL)inBackground {
  DCHECK(_browserState);
  base::scoped_nsobject<Tab> tab([[Tab alloc] initWithBrowserState:_browserState
                                                            opener:parentTab
                                                       openedByDOM:openedByDOM
                                                             model:self]);
  [tab webController].webUsageEnabled = webUsageEnabled_;

  [self insertTab:tab
          atIndex:index
           opener:parentTab
       transition:params.transition_type];

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

- (void)insertTab:(Tab*)tab
          atIndex:(NSUInteger)index
           opener:(Tab*)parentTab
       transition:(ui::PageTransition)transition {
  DCHECK(tab);
  DCHECK(![_tabRetainer containsObject:tab]);

  [_tabRetainer addObject:tab];
  if (index == TabModelConstants::kTabPositionAutomatically) {
    _webStateList.AppendWebState(transition, tab.webState, parentTab.webState);
  } else {
    DCHECK_LE(index, static_cast<NSUInteger>(INT_MAX));
    const int insertion_index = static_cast<int>(index);
    _webStateList.InsertWebState(insertion_index, tab.webState,
                                 parentTab.webState);
  }

  // Persist the session due to a new tab being inserted. If this is a
  // background tab (will not become active), saving now will capture the
  // state properly. If it does eventually become active, another save will
  // be triggered to properly capture the end result.
  [self saveSessionImmediately:NO];
}

- (void)insertTab:(Tab*)tab atIndex:(NSUInteger)index opener:(Tab*)parentTab {
  DCHECK(tab);
  DCHECK(![_tabRetainer containsObject:tab]);
  DCHECK_LE(index, static_cast<NSUInteger>(INT_MAX));

  [self insertTab:tab
          atIndex:index
           opener:parentTab
       transition:ui::PAGE_TRANSITION_GENERATED];
}

- (void)insertTab:(Tab*)tab atIndex:(NSUInteger)index {
  DCHECK(tab);
  DCHECK(![_tabRetainer containsObject:tab]);
  DCHECK_LE(index, static_cast<NSUInteger>(INT_MAX));

  [self insertTab:tab atIndex:index opener:GetOpenerForTab(self, tab)];
}

- (void)moveTab:(Tab*)tab toIndex:(NSUInteger)toIndex {
  DCHECK([_tabRetainer containsObject:tab]);
  DCHECK_LE(toIndex, static_cast<NSUInteger>(INT_MAX));
  int fromIndex = _webStateList.GetIndexOfWebState(tab.webState);
  _webStateList.MoveWebStateAt(fromIndex, static_cast<int>(toIndex));
}

- (void)replaceTab:(Tab*)oldTab withTab:(Tab*)newTab {
  DCHECK([_tabRetainer containsObject:oldTab]);
  DCHECK(![_tabRetainer containsObject:newTab]);

  int index = _webStateList.GetIndexOfWebState(oldTab.webState);
  DCHECK_NE(index, WebStateList::kInvalidIndex);
  DCHECK_GE(index, 0);

  base::scoped_nsobject<Tab> tabSaver([oldTab retain]);
  [_tabRetainer removeObject:oldTab];
  [_tabRetainer addObject:newTab];
  [newTab setParentTabModel:self];

  // The WebState is owned by the associated Tab, so it is safe to ignore
  // the result and won't cause a memory leak. Once the ownership is moved
  // to WebStateList, this function will return a std::unique_ptr<> and the
  // object destroyed as expected, so it will fine to ignore the result then
  // too. See http://crbug.com/546222 for progress of changing the ownership
  // of the WebStates.
  ignore_result(_webStateList.ReplaceWebStateAt(
      index, newTab.webState, GetOpenerForTab(self, newTab).webState));

  [oldTab setParentTabModel:nil];
  [oldTab close];
}

- (void)closeTabAtIndex:(NSUInteger)index {
  DCHECK(index < self.count);
  [self closeTab:[self tabAtIndex:index]];
}

- (void)closeTab:(Tab*)tab {
  // Ensure the tab stays alive long enough for us to send out the
  // notice of its destruction to the delegate.
  [_observers tabModel:self willRemoveTab:tab];
  [tab close];  // Note it is not safe to access the tab after 'close'.
}

- (void)closeAllTabs {
  for (NSInteger i = self.count - 1; i >= 0; --i)
    [self closeTabAtIndex:i];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kTabModelAllTabsDidCloseNotification
                    object:self];
}

- (void)haltAllTabs {
  for (Tab* tab in self) {
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
  if (_webStateListMetricsObserver)
    _webStateListMetricsObserver->ResetSessionMetrics();
}

- (void)recordSessionMetrics {
  if (_webStateListMetricsObserver)
    _webStateListMetricsObserver->RecordSessionMetrics();
}

- (void)notifyTabSnapshotChanged:(Tab*)tab withImage:(UIImage*)image {
  DCHECK([NSThread isMainThread]);
  [_observers tabModel:self didChangeTabSnapshot:tab withImage:image];
}

- (void)resetAllWebViews {
  for (Tab* tab in self) {
    [tab.webController reinitializeWebViewAndReload:(tab == self.currentTab)];
  }
}

- (void)setWebUsageEnabled:(BOOL)webUsageEnabled {
  if (webUsageEnabled_ == webUsageEnabled)
    return;
  webUsageEnabled_ = webUsageEnabled;
  for (Tab* tab in self) {
    tab.webUsageEnabled = webUsageEnabled;
  }
}

- (void)setPrimary:(BOOL)primary {
  if (_tabUsageRecorder)
    _tabUsageRecorder->RecordPrimaryTabModelChange(primary, self.currentTab);
}

- (NSSet*)currentlyReferencedExternalFiles {
  NSMutableSet* referencedFiles = [NSMutableSet set];
  if (!_browserState)
    return referencedFiles;
  // Check the currently open tabs for external files.
  for (Tab* tab in self) {
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
  DCHECK(closedTab);
  DCHECK([_tabRetainer containsObject:closedTab]);
  int closedTabIndex = _webStateList.GetIndexOfWebState(closedTab.webState);
  DCHECK_NE(closedTabIndex, WebStateList::kInvalidIndex);
  DCHECK_GE(closedTabIndex, 0);

  // Let the sessions::TabRestoreService know about that new tab.
  sessions::TabRestoreService* restoreService =
      _browserState
          ? IOSChromeTabRestoreServiceFactory::GetForBrowserState(_browserState)
          : nullptr;
  web::NavigationManager* navigationManager = [closedTab navigationManager];
  DCHECK(navigationManager);
  int itemCount = navigationManager->GetItemCount();
  if (restoreService && (![self isNTPTab:closedTab] || itemCount > 1)) {
    restoreService->CreateHistoricalTab(
        sessions::IOSLiveTab::GetForWebState(closedTab.webState),
        closedTabIndex);
  }

  base::scoped_nsobject<Tab> kungFuDeathGrip([closedTab retain]);

  // If a non-current Tab is closed, save the session (it will be saved by
  // TabModelObserversBridge if the currentTab has been closed).
  BOOL needToSaveSession = (closedTab != self.currentTab);

  DCHECK([_tabRetainer containsObject:closedTab]);
  [_tabRetainer removeObject:closedTab];

  // The WebState is owned by the associated Tab, so it is safe to ignore
  // the result and won't cause a memory leak. Once the ownership is moved
  // to WebStateList, this function will return a std::unique_ptr<> and the
  // object destroyed as expected, so it will fine to ignore the result then
  // too. See http://crbug.com/546222 for progress of changing the ownership
  // of the WebStates.
  ignore_result(_webStateList.DetachWebStateAt(closedTabIndex));

  if (needToSaveSession)
    [self saveSessionImmediately:NO];
}

- (void)navigationCommittedInTab:(Tab*)tab
                    previousItem:(web::NavigationItem*)previousItem {
  if (self.offTheRecord)
    return;
  if (![tab navigationManager])
    return;

  // See if the navigation was within a page; if so ignore it.
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
  return [_fastEnumerationHelper countByEnumeratingWithState:state
                                                     objects:objects
                                                       count:count];
}

#pragma mark - TabUsageRecorderDelegate

- (NSUInteger)liveTabsCount {
  NSUInteger count = 0;
  for (Tab* tab in self) {
    if ([tab.webController isViewAlive])
      count++;
  }
  return count;
}

#pragma mark - Private methods

- (SessionWindowIOS*)windowForSavingSession {
  // Background tabs will already have their state preserved, but not the
  // fg tab. Do it now.
  [self.currentTab recordStateInHistory];

  // Build the array of sessions. Copy the session objects as the saving will
  // be done on a separate thread.
  // TODO(crbug.com/661986): This could get expensive especially since this
  // window may never be saved (if another call comes in before the delay).
  SessionWindowIOS* window = [[[SessionWindowIOS alloc] init] autorelease];
  for (Tab* tab in self) {
    web::WebState* webState = tab.webState;
    DCHECK(webState);
    [window addSerializedSessionStorage:webState->BuildSessionStorage()];
  }
  window.selectedIndex = [self indexOfTab:self.currentTab];
  return window;
}

- (BOOL)isNTPTab:(Tab*)tab {
  std::string host = tab.url.host();
  return host == kChromeUINewTabHost || host == kChromeUIBookmarksHost;
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

  int oldCount = _webStateList.count();
  DCHECK_GE(oldCount, 0);

  web::WebState::CreateParams params(_browserState);
  scoped_refptr<web::CertificatePolicyCache> policyCache =
      web::BrowserState::GetCertificatePolicyCache(_browserState);

  base::scoped_nsobject<NSMutableArray<Tab*>> restoredTabs(
      [[NSMutableArray alloc] initWithCapacity:sessions.count]);

  // Recreate all the restored Tabs and add them to the WebStateList without
  // any opener-opened relationship (as the n-th restored Tab opener may be
  // at an index larger than n). Then in a second pass fix the openers.
  for (CRWSessionStorage* session in sessions) {
    std::unique_ptr<web::WebState> webState =
        web::WebState::Create(params, session);
    base::scoped_nsobject<Tab> tab(
        [[Tab alloc] initWithWebState:std::move(webState) model:self]);
    [tab webController].webUsageEnabled = webUsageEnabled_;
    [tab webController].usePlaceholderOverlay = YES;

    // Restore the CertificatePolicyCache (note that webState is invalid after
    // passing it via move semantic to -insertTabWithWebState:atIndex:).
    UpdateCertificatePolicyCacheFromWebState(policyCache, [tab webState]);
    [self insertTab:tab atIndex:self.count opener:nil];
    [restoredTabs addObject:tab.get()];
  }

  DCHECK_EQ(sessions.count, [restoredTabs count]);
  DCHECK_GT(_webStateList.count(), oldCount);

  // Fix openers now that all Tabs have been restored. Only look for an opener
  // Tab in the newly restored Tabs and not in the already open Tabs.
  for (int index = oldCount; index < _webStateList.count(); ++index) {
    DCHECK_GE(index, oldCount);
    NSUInteger tabIndex = static_cast<NSUInteger>(index - oldCount);
    Tab* tab = [restoredTabs objectAtIndex:tabIndex];
    Tab* opener = GetOpenerForTab(restoredTabs.get(), tab);
    if (opener) {
      DCHECK(opener.webState);
      _webStateList.SetOpenerOfWebStateAt(index, opener.webState);
    }
  }

  // Update the selected tab if there was a selected Tab in the saved session.
  if (window.selectedIndex != NSNotFound) {
    NSUInteger selectedIndex = window.selectedIndex + oldCount;
    DCHECK_LT(selectedIndex, self.count);
    DCHECK([self tabAtIndex:selectedIndex]);

    if (persistState && self.currentTab)
      [self.currentTab recordStateInHistory];
    _webStateList.ActivateWebStateAt(static_cast<int>(selectedIndex));
  }

  // If there was only one tab and it was the new tab page, clobber it.
  BOOL closedNTPTab = NO;
  if (oldCount == 1) {
    Tab* tab = [self tabAtIndex:0];
    if (tab.url == GURL(kChromeUINewTabURL)) {
      [self closeTab:tab];
      closedNTPTab = YES;
      oldCount = 0;
    }
  }
  if (_tabUsageRecorder) {
    NSMutableArray<Tab*>* restoredTabs =
        [NSMutableArray arrayWithCapacity:_webStateList.count() - oldCount];
    for (int index = oldCount; index < _webStateList.count(); ++index) {
      web::WebState* webState = _webStateList.GetWebStateAt(index);
      [restoredTabs addObject:LegacyTabHelper::GetTabForWebState(webState)];
    }
    _tabUsageRecorder->InitialRestoredTabs(self.currentTab, restoredTabs);
  }
  return closedNTPTab;
}

#pragma mark - Notification Handlers

// Called when UIApplicationWillResignActiveNotification is received.
- (void)willResignActive:(NSNotification*)notify {
  if (webUsageEnabled_ && self.currentTab) {
    [[SnapshotCache sharedInstance]
        willBeSavedGreyWhenBackgrounding:self.currentTab.tabId];
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
      web::BrowserState::GetCertificatePolicyCache(_browserState),
      &_webStateList);

  if (_tabUsageRecorder)
    _tabUsageRecorder->AppDidEnterBackground();

  // Normally, the session is saved after some timer expires but since the app
  // is about to enter the background send YES to save the session immediately.
  [self saveSessionImmediately:YES];

  // Write out a grey version of the current website to disk.
  if (webUsageEnabled_ && self.currentTab) {
    [[SnapshotCache sharedInstance]
        saveGreyInBackgroundForSessionID:self.currentTab.tabId];
  }
}

// Called when UIApplicationWillEnterForegroundNotification is received.
- (void)applicationWillEnterForeground:(NSNotification*)notify {
  if (_tabUsageRecorder) {
    _tabUsageRecorder->AppWillEnterForeground();
  }
}

@end
