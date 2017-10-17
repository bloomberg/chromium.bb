// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/ios/web_favicon_driver.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/ios/ios_live_tab.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/chrome_url_util.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_web_state_list_observer.h"
#include "ios/chrome/browser/tab_parenting_global_observer.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model_closing_web_state_observer.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#import "ios/chrome/browser/tabs/tab_model_notification_observer.h"
#import "ios/chrome/browser/tabs/tab_model_observers.h"
#import "ios/chrome/browser/tabs/tab_model_observers_bridge.h"
#import "ios/chrome/browser/tabs/tab_model_selected_tab_observer.h"
#import "ios/chrome/browser/tabs/tab_model_synced_window_delegate.h"
#import "ios/chrome/browser/tabs/tab_model_web_state_list_delegate.h"
#import "ios/chrome/browser/tabs/tab_model_web_usage_enabled_observer.h"
#import "ios/chrome/browser/tabs/tab_parenting_observer.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_fast_enumeration_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list_metrics_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list_serialization.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/certificate_policy_cache.h"
#include "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/serializable_user_data_manager.h"
#include "ios/web/public/web_state/session_certificate_policy_cache.h"
#include "ios/web/public/web_thread.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
    const web::WebState* web_state) {
  DCHECK(web_state);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  web_state->GetSessionCertificatePolicyCache()->UpdateCertificatePolicyCache(
      policy_cache);
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

}  // anonymous namespace

@interface TabModelWebStateProxyFactory : NSObject<WebStateProxyFactory>
@end

@implementation TabModelWebStateProxyFactory

- (id)proxyForWebState:(web::WebState*)webState {
  return LegacyTabHelper::GetTabForWebState(webState);
}

@end

@interface TabModel () {
  // Delegate for the WebStateList.
  std::unique_ptr<WebStateListDelegate> _webStateListDelegate;

  // Underlying shared model implementation.
  std::unique_ptr<WebStateList> _webStateList;

  // Helper providing NSFastEnumeration implementation over the WebStateList.
  std::unique_ptr<WebStateListFastEnumerationHelper> _fastEnumerationHelper;

  // WebStateListObservers reacting to modifications of the model (may send
  // notification, translate and forward events, update metrics, ...).
  std::vector<std::unique_ptr<WebStateListObserver>> _webStateListObservers;

  // Strong references to id<WebStateListObserving> wrapped by non-owning
  // WebStateListObserverBridges.
  NSArray<id<WebStateListObserving>>* _retainedWebStateListObservers;

  // The delegate for sync.
  std::unique_ptr<TabModelSyncedWindowDelegate> _syncedWindowDelegate;

  // The observer that sends kTabModelNewTabWillOpenNotification notifications.
  TabModelNotificationObserver* _tabModelNotificationObserver;

  // Counters for metrics.
  WebStateListMetricsObserver* _webStateListMetricsObserver;

  // Backs up property with the same name.
  std::unique_ptr<TabUsageRecorder> _tabUsageRecorder;
  // Backs up property with the same name.
  const SessionID _sessionID;
  // Saves session's state.
  SessionServiceIOS* _sessionService;
  // List of TabModelObservers.
  TabModelObservers* _observers;

  // Used to ensure thread-safety of the certificate policy management code.
  base::CancelableTaskTracker _clearPoliciesTaskTracker;
}

// Session window for the contents of the tab model.
@property(nonatomic, readonly) SessionIOS* sessionForSaving;

// Helper method to restore a saved session and control if the state should
// be persisted or not. Used to implement the public -restoreSessionWindow:
// method and restoring session in the initialiser.
- (BOOL)restoreSessionWindow:(SessionWindowIOS*)window
                persistState:(BOOL)persistState;

@end

@implementation TabModel

@synthesize browserState = _browserState;
@synthesize sessionID = _sessionID;
@synthesize webUsageEnabled = _webUsageEnabled;

#pragma mark - Overriden

- (void)dealloc {
  // browserStateDestroyed should always have been called before destruction.
  DCHECK(!_browserState);

  // Make sure the observers do clean after themselves.
  DCHECK([_observers empty]);
}

#pragma mark - Public methods

- (Tab*)currentTab {
  web::WebState* webState = _webStateList->GetActiveWebState();
  return webState ? LegacyTabHelper::GetTabForWebState(webState) : nil;
}

- (void)setCurrentTab:(Tab*)newTab {
  int indexOfTab = _webStateList->GetIndexOfWebState(newTab.webState);
  DCHECK_NE(indexOfTab, WebStateList::kInvalidIndex);
  _webStateList->ActivateWebStateAt(indexOfTab);
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
  return _webStateList->empty();
}

- (NSUInteger)count {
  DCHECK_GE(_webStateList->count(), 0);
  return static_cast<NSUInteger>(_webStateList->count());
}

- (WebStateList*)webStateList {
  return _webStateList.get();
}

- (instancetype)initWithSessionWindow:(SessionWindowIOS*)window
                       sessionService:(SessionServiceIOS*)service
                         browserState:(ios::ChromeBrowserState*)browserState {
  if ((self = [super init])) {
    _observers = [TabModelObservers observers];

    _webStateListDelegate =
        base::MakeUnique<TabModelWebStateListDelegate>(self);
    _webStateList = base::MakeUnique<WebStateList>(_webStateListDelegate.get());

    _fastEnumerationHelper =
        base::MakeUnique<WebStateListFastEnumerationHelper>(
            _webStateList.get(), [[TabModelWebStateProxyFactory alloc] init]);

    _browserState = browserState;
    DCHECK(_browserState);

    // Normal browser states are the only ones to get tab restore. Tab sync
    // handles incognito browser states by filtering on profile, so it's
    // important to the backend code to always have a sync window delegate.
    if (!_browserState->IsOffTheRecord()) {
      // Set up the usage recorder before tabs are created.
      _tabUsageRecorder = base::MakeUnique<TabUsageRecorder>(
          _webStateList.get(),
          PrerenderServiceFactory::GetForBrowserState(browserState));
    }
    _syncedWindowDelegate = base::MakeUnique<TabModelSyncedWindowDelegate>(
        _webStateList.get(), _sessionID);

    // There must be a valid session service defined to consume session windows.
    DCHECK(service);
    _sessionService = service;

    NSMutableArray<id<WebStateListObserving>>* retainedWebStateListObservers =
        [[NSMutableArray alloc] init];

    TabModelClosingWebStateObserver* tabModelClosingWebStateObserver = [
        [TabModelClosingWebStateObserver alloc]
        initWithTabModel:self
          restoreService:IOSChromeTabRestoreServiceFactory::GetForBrowserState(
                             _browserState)];
    [retainedWebStateListObservers addObject:tabModelClosingWebStateObserver];

    _webStateListObservers.push_back(
        base::MakeUnique<WebStateListObserverBridge>(
            tabModelClosingWebStateObserver));

    SnapshotCache* snapshotCache =
        SnapshotCacheFactory::GetForBrowserState(_browserState);
    if (snapshotCache) {
      _webStateListObservers.push_back(
          base::MakeUnique<SnapshotCacheWebStateListObserver>(snapshotCache));
    }

    _webStateListObservers.push_back(base::MakeUnique<TabParentingObserver>());

    TabModelSelectedTabObserver* tabModelSelectedTabObserver =
        [[TabModelSelectedTabObserver alloc] initWithTabModel:self];
    [retainedWebStateListObservers addObject:tabModelSelectedTabObserver];
    _webStateListObservers.push_back(
        base::MakeUnique<WebStateListObserverBridge>(
            tabModelSelectedTabObserver));

    TabModelObserversBridge* tabModelObserversBridge =
        [[TabModelObserversBridge alloc] initWithTabModel:self
                                        tabModelObservers:_observers];
    [retainedWebStateListObservers addObject:tabModelObserversBridge];
    _webStateListObservers.push_back(
        base::MakeUnique<WebStateListObserverBridge>(tabModelObserversBridge));

    auto webStateListMetricsObserver =
        base::MakeUnique<WebStateListMetricsObserver>();
    _webStateListMetricsObserver = webStateListMetricsObserver.get();
    _webStateListObservers.push_back(std::move(webStateListMetricsObserver));

    _webStateListObservers.push_back(
        base::MakeUnique<TabModelWebUsageEnabledObserver>(self));

    auto tabModelNotificationObserver =
        base::MakeUnique<TabModelNotificationObserver>(self);
    _tabModelNotificationObserver = tabModelNotificationObserver.get();
    _webStateListObservers.push_back(std::move(tabModelNotificationObserver));

    for (const auto& webStateListObserver : _webStateListObservers)
      _webStateList->AddObserver(webStateListObserver.get());
    _retainedWebStateListObservers = [retainedWebStateListObservers copy];

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
  if ((!self.currentTab && _webStateList->count()) || !_browserState)
    return;
  NSString* statePath =
      base::SysUTF8ToNSString(_browserState->GetStatePath().AsUTF8Unsafe());
  __weak TabModel* weakSelf = self;
  SessionIOSFactory sessionFactory = ^{
    return weakSelf.sessionForSaving;
  };
  [_sessionService saveSession:sessionFactory
                     directory:statePath
                   immediately:immediately];
}

- (Tab*)tabAtIndex:(NSUInteger)index {
  DCHECK_LE(index, static_cast<NSUInteger>(INT_MAX));
  return LegacyTabHelper::GetTabForWebState(
      _webStateList->GetWebStateAt(static_cast<int>(index)));
}

- (NSUInteger)indexOfTab:(Tab*)tab {
  int index = _webStateList->GetIndexOfWebState(tab.webState);
  if (index == WebStateList::kInvalidIndex)
    return NSNotFound;

  DCHECK_GE(index, 0);
  return static_cast<NSUInteger>(index);
}

- (Tab*)openerOfTab:(Tab*)tab {
  int index = _webStateList->GetIndexOfWebState(tab.webState);
  if (index == WebStateList::kInvalidIndex)
    return nil;

  WebStateOpener opener = _webStateList->GetOpenerOfWebStateAt(index);
  return opener.opener ? LegacyTabHelper::GetTabForWebState(opener.opener)
                       : nil;
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
            (const web::NavigationManager::WebLoadParams&)loadParams
                         opener:(Tab*)parentTab
                    openedByDOM:(BOOL)openedByDOM
                        atIndex:(NSUInteger)index
                   inBackground:(BOOL)inBackground {
  DCHECK(_browserState);

  int insertionIndex = WebStateList::kInvalidIndex;
  int insertionFlags = WebStateList::INSERT_NO_FLAGS;
  if (index != TabModelConstants::kTabPositionAutomatically) {
    DCHECK_LE(index, static_cast<NSUInteger>(INT_MAX));
    insertionIndex = static_cast<int>(index);
    insertionFlags |= WebStateList::INSERT_FORCE_INDEX;
  } else if (!ui::PageTransitionCoreTypeIs(loadParams.transition_type,
                                           ui::PAGE_TRANSITION_LINK)) {
    insertionIndex = _webStateList->count();
    insertionFlags |= WebStateList::INSERT_FORCE_INDEX;
  }

  if (!inBackground) {
    insertionFlags |= WebStateList::INSERT_ACTIVATE;
  }

  web::WebState::CreateParams createParams(self.browserState);
  createParams.created_with_opener = openedByDOM;

  std::unique_ptr<web::WebState> webState = web::WebState::Create(createParams);
  webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  insertionIndex = _webStateList->InsertWebState(
      insertionIndex, std::move(webState), insertionFlags,
      WebStateOpener(parentTab.webState));

  return LegacyTabHelper::GetTabForWebState(
      _webStateList->GetWebStateAt(insertionIndex));
}

- (void)moveTab:(Tab*)tab toIndex:(NSUInteger)toIndex {
  DCHECK_LE(toIndex, static_cast<NSUInteger>(INT_MAX));
  int fromIndex = _webStateList->GetIndexOfWebState(tab.webState);
  _webStateList->MoveWebStateAt(fromIndex, static_cast<int>(toIndex));
}

- (void)closeTabAtIndex:(NSUInteger)index {
  DCHECK_LE(index, static_cast<NSUInteger>(INT_MAX));
  _webStateList->CloseWebStateAt(static_cast<int>(index));
}

- (void)closeTab:(Tab*)tab {
  [self closeTabAtIndex:[self indexOfTab:tab]];
}

- (void)closeAllTabs {
  _webStateList->CloseAllWebStates();
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

- (void)setWebUsageEnabled:(BOOL)webUsageEnabled {
  if (_webUsageEnabled == webUsageEnabled)
    return;
  _webUsageEnabled = webUsageEnabled;
  for (int index = 0; index < _webStateList->count(); ++index) {
    web::WebState* webState = _webStateList->GetWebStateAt(index);
    webState->SetWebUsageEnabled(_webUsageEnabled ? true : false);
  }
}

- (void)setPrimary:(BOOL)primary {
  if (_tabUsageRecorder) {
    _tabUsageRecorder->RecordPrimaryTabModelChange(primary,
                                                   self.currentTab.webState);
  }
}

- (NSSet*)currentlyReferencedExternalFiles {
  NSMutableSet* referencedFiles = [NSMutableSet set];
  if (!_browserState)
    return referencedFiles;
  // Check the currently open tabs for external files.
  for (Tab* tab in self) {
    const GURL& lastCommittedURL = tab.webState->GetLastCommittedURL();
    if (UrlIsExternalFileReference(lastCommittedURL)) {
      [referencedFiles addObject:base::SysUTF8ToNSString(
                                     lastCommittedURL.ExtractFileName())];
    }
    web::NavigationItem* pendingItem =
        tab.webState->GetNavigationManager()->GetPendingItem();
    if (pendingItem && UrlIsExternalFileReference(pendingItem->GetURL())) {
      [referencedFiles addObject:base::SysUTF8ToNSString(
                                     pendingItem->GetURL().ExtractFileName())];
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
  if (!_browserState)
    return;

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  UnregisterTabModelFromChromeBrowserState(_browserState, self);
  _browserState = nullptr;

  // Clear weak pointer to observers before destroying them.
  _tabModelNotificationObserver = nullptr;
  _webStateListMetricsObserver = nullptr;

  // Close all tabs. Do this in an @autoreleasepool as WebStateList observers
  // will be notified (they are unregistered later). As some of them may be
  // implemented in Objective-C and unregister themselves in their -dealloc
  // method, ensure they -autorelease introduced by ARC are processed before
  // the WebStateList destructor is called.
  @autoreleasepool {
    [self closeAllTabs];
  }

  // Unregister all observers after closing all the tabs as some of them are
  // required to properly clean up the Tabs.
  for (const auto& webStateListObserver : _webStateListObservers)
    _webStateList->RemoveObserver(webStateListObserver.get());
  _webStateListObservers.clear();
  _retainedWebStateListObservers = nil;

  _clearPoliciesTaskTracker.TryCancelAll();
  _tabUsageRecorder.reset();
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
                                  objects:(id __unsafe_unretained*)objects
                                    count:(NSUInteger)count {
  return [_fastEnumerationHelper->GetFastEnumeration()
      countByEnumeratingWithState:state
                          objects:objects
                            count:count];
}

#pragma mark - Private methods

- (SessionIOS*)sessionForSaving {
  // Build the array of sessions. Copy the session objects as the saving will
  // be done on a separate thread.
  // TODO(crbug.com/661986): This could get expensive especially since this
  // window may never be saved (if another call comes in before the delay).
  return [[SessionIOS alloc]
      initWithWindows:@[ SerializeWebStateList(_webStateList.get()) ]];
}

- (BOOL)restoreSessionWindow:(SessionWindowIOS*)window
                persistState:(BOOL)persistState {
  DCHECK(_browserState);
  DCHECK(window);

  // Disable sending the kTabModelNewTabWillOpenNotification notification
  // while restoring a session as it breaks the BVC (see crbug.com/763964).
  base::ScopedClosureRunner enableTabModelNotificationObserver;
  if (_tabModelNotificationObserver) {
    _tabModelNotificationObserver->SetDisabled(true);
    enableTabModelNotificationObserver.ReplaceClosure(
        base::BindOnce(&TabModelNotificationObserver::SetDisabled,
                       base::Unretained(_tabModelNotificationObserver), false));
  }

  if (!window.sessions.count)
    return NO;

  int oldCount = _webStateList->count();
  DCHECK_GE(oldCount, 0);

  web::WebState::CreateParams createParams(_browserState);
  DeserializeWebStateList(
      _webStateList.get(), window,
      base::BindRepeating(&web::WebState::CreateWithStorageSession,
                          createParams));

  DCHECK_GT(_webStateList->count(), oldCount);
  int restoredCount = _webStateList->count() - oldCount;
  DCHECK_EQ(window.sessions.count, static_cast<NSUInteger>(restoredCount));

  scoped_refptr<web::CertificatePolicyCache> policyCache =
      web::BrowserState::GetCertificatePolicyCache(_browserState);

  std::vector<web::WebState*> restoredWebStates;
  if (_tabUsageRecorder)
    restoredWebStates.reserve(window.sessions.count);

  for (int index = oldCount; index < _webStateList->count(); ++index) {
    web::WebState* webState = _webStateList->GetWebStateAt(index);
    web::NavigationItem* visible_item =
        webState->GetNavigationManager()->GetVisibleItem();

    if (!(visible_item &&
          visible_item->GetVirtualURL() == GURL(kChromeUINewTabURL))) {
      PagePlaceholderTabHelper::FromWebState(webState)
          ->AddPlaceholderForNextNavigation();
    }

    if (visible_item && visible_item->GetVirtualURL().is_valid()) {
      favicon::WebFaviconDriver::FromWebState(webState)->FetchFavicon(
          visible_item->GetVirtualURL(), /*is_same_document=*/false);
    }

    // Restore the CertificatePolicyCache (note that webState is invalid after
    // passing it via move semantic to -initWithWebState:model:).
    UpdateCertificatePolicyCacheFromWebState(policyCache, webState);

    if (_tabUsageRecorder)
      restoredWebStates.push_back(webState);
  }

  // If there was only one tab and it was the new tab page, clobber it.
  BOOL closedNTPTab = NO;
  if (oldCount == 1) {
    Tab* tab = [self tabAtIndex:0];
    BOOL hasPendingLoad =
        tab.webState->GetNavigationManager()->GetPendingItem() != nullptr;
    if (!hasPendingLoad &&
        tab.webState->GetLastCommittedURL() == kChromeUINewTabURL) {
      [self closeTab:tab];
      closedNTPTab = YES;
      oldCount = 0;
    }
  }
  if (_tabUsageRecorder) {
    _tabUsageRecorder->InitialRestoredTabs(self.currentTab.webState,
                                           restoredWebStates);
  }

  return closedNTPTab;
}

#pragma mark - Notification Handlers

// Called when UIApplicationWillResignActiveNotification is received.
- (void)willResignActive:(NSNotification*)notify {
  if (_webUsageEnabled && self.currentTab) {
    [SnapshotCacheFactory::GetForBrowserState(_browserState)
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
      _webStateList.get());

  // Normally, the session is saved after some timer expires but since the app
  // is about to enter the background send YES to save the session immediately.
  [self saveSessionImmediately:YES];

  // Write out a grey version of the current website to disk.
  if (_webUsageEnabled && self.currentTab) {
    [SnapshotCacheFactory::GetForBrowserState(_browserState)
        saveGreyInBackgroundForSessionID:self.currentTab.tabId];
  }
}

@end
