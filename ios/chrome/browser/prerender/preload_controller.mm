// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/prerender/preload_controller.h"

#include "base/ios/device_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/prefs/pref_service.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#import "ios/chrome/browser/history/history_tab_helper.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prerender/preload_controller_delegate.h"
#import "ios/chrome/browser/signin/account_consistency_service_factory.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/tabs/tab_private.h"
#include "ios/chrome/browser/ui/prerender_final_status.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#import "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Delay before starting to prerender a URL.
const NSTimeInterval kPrerenderDelay = 0.5;

// The finch experiment to turn off prerendering as a field trial.
const char kTabEvictionFieldTrialName[] = "TabEviction";
// The associated group.
const char kPrerenderTabEvictionTrialGroup[] = "NoPrerendering";
// The name of the histogram for recording final status (e.g. used/cancelled)
// of prerender requests.
const char kPrerenderFinalStatusHistogramName[] = "Prerender.FinalStatus";
// The name of the histogram for recording the number of successful prerenders.
const char kPrerendersPerSessionCountHistogramName[] =
    "Prerender.PrerendersPerSessionCount";

// Is this install selected for this particular experiment.
bool IsPrerenderTabEvictionExperimentalGroup() {
  base::FieldTrial* trial =
      base::FieldTrialList::Find(kTabEvictionFieldTrialName);
  return trial && trial->group_name() == kPrerenderTabEvictionTrialGroup;
}

}  // namespace

@interface PreloadController (PrivateMethods)<ManageAccountsDelegate>

// Returns YES if prerendering is enabled.
- (BOOL)isPrerenderingEnabled;

// Returns YES if the |url| is valid for prerendering.
- (BOOL)shouldPreloadURL:(const GURL&)url;

// Called to start any scheduled prerendering requests.
- (void)startPrerender;

// Destroys the preview Tab and resets |prerenderURL_| to the empty URL.
- (void)destroyPreviewContents;

// Schedules the current prerender to be cancelled during the next run of the
// event loop.
- (void)schedulePrerenderCancel;

// Removes any scheduled prerender requests and resets |scheduledURL| to the
// empty URL.
- (void)removeScheduledPrerenderRequests;

@end

@implementation PreloadController {
  ios::ChromeBrowserState* browserState_;  // Weak.

  // The WebState used for prerendering.
  std::unique_ptr<web::WebState> webState_;

  // The WebStateDelegateBridge used to register self as a CRWWebStateDelegate
  // with the pre-rendered WebState.
  std::unique_ptr<web::WebStateDelegateBridge> webStateDelegate_;

  // The URL that is prerendered in |webState_|.  This can be different from
  // the value returned by WebState last committed navigation item, for example
  // in cases where there was a redirect.
  //
  // When choosing whether or not to use a prerendered Tab,
  // BrowserViewController compares the URL being loaded by the omnibox with the
  // URL of the prerendered Tab.  Comparing against the Tab's currently URL
  // could return false negatives in cases of redirect, hence the need to store
  // the originally prerendered URL.
  GURL prerenderedURL_;

  // The URL that is scheduled to be prerendered, its associated transition and
  // referrer. |scheduledTransition_| and |scheduledReferrer_| are not valid
  // when |scheduledURL_| is empty.
  GURL scheduledURL_;
  ui::PageTransition scheduledTransition_;
  web::Referrer scheduledReferrer_;

  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> observerBridge_;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar prefChangeRegistrar_;
  // Observer for the WWAN setting.  Contains a valid object only if the
  // instant setting is set to wifi-only.
  std::unique_ptr<ConnectionTypeObserverBridge> connectionTypeObserverBridge_;

  // Whether or not the preference is enabled.
  BOOL enabled_;
  // Whether or not prerendering is only when on wifi.
  BOOL wifiOnly_;
  // Whether or not the current connection is using WWAN.
  BOOL usingWWAN_;

  // Number of successful prerenders (i.e. the user viewed the prerendered page)
  // during the lifetime of this controller.
  int successfulPrerendersPerSessionCount_;
}

@synthesize prerenderedURL = prerenderedURL_;
@synthesize delegate = delegate_;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if ((self = [super init])) {
    browserState_ = browserState;
    enabled_ =
        browserState_->GetPrefs()->GetBoolean(prefs::kNetworkPredictionEnabled);
    wifiOnly_ = browserState_->GetPrefs()->GetBoolean(
        prefs::kNetworkPredictionWifiOnly);
    usingWWAN_ = net::NetworkChangeNotifier::IsConnectionCellular(
        net::NetworkChangeNotifier::GetConnectionType());
    webStateDelegate_.reset(new web::WebStateDelegateBridge(self));
    observerBridge_.reset(new PrefObserverBridge(self));
    prefChangeRegistrar_.Init(browserState_->GetPrefs());
    observerBridge_->ObserveChangesForPreference(
        prefs::kNetworkPredictionEnabled, &prefChangeRegistrar_);
    observerBridge_->ObserveChangesForPreference(
        prefs::kNetworkPredictionWifiOnly, &prefChangeRegistrar_);
    if (enabled_ && wifiOnly_) {
      connectionTypeObserverBridge_.reset(
          new ConnectionTypeObserverBridge(self));
    }

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(didReceiveMemoryWarning)
               name:UIApplicationDidReceiveMemoryWarningNotification
             object:nil];
  }
  return self;
}

- (void)browserStateDestroyed {
  [self cancelPrerender];
  connectionTypeObserverBridge_.reset();
}

- (void)dealloc {
  UMA_HISTOGRAM_COUNTS(kPrerendersPerSessionCountHistogramName,
                       successfulPrerendersPerSessionCount_);
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self cancelPrerender];
}

- (void)prerenderURL:(const GURL&)url
            referrer:(const web::Referrer&)referrer
          transition:(ui::PageTransition)transition
         immediately:(BOOL)immediately {
  // TODO(crbug.com/754050): If shouldPrerenderURL returns false, should we
  // cancel any scheduled prerender requests?
  if (![self isPrerenderingEnabled] || ![self shouldPreloadURL:url])
    return;

  // Ignore this request if there is already a scheduled request for the same
  // URL; or, if there is no scheduled request, but the currently prerendered
  // page matches this URL.
  if (url == scheduledURL_ ||
      (scheduledURL_.is_empty() && url == prerenderedURL_)) {
    return;
  }

  [self removeScheduledPrerenderRequests];
  scheduledURL_ = url;
  scheduledTransition_ = transition;
  scheduledReferrer_ = referrer;

  NSTimeInterval delay = immediately ? 0.0 : kPrerenderDelay;
  [self performSelector:@selector(startPrerender)
             withObject:nil
             afterDelay:delay];
}

- (void)cancelPrerender {
  [self cancelPrerenderForReason:PRERENDER_FINAL_STATUS_CANCELLED];
}

- (void)cancelPrerenderForReason:(PrerenderFinalStatus)reason {
  [self removeScheduledPrerenderRequests];
  [self destroyPreviewContentsForReason:reason];
}

- (BOOL)isWebStatePrerendered:(web::WebState*)webState {
  return webState && webState_.get() == webState;
}

- (std::unique_ptr<web::WebState>)releasePrerenderContents {
  successfulPrerendersPerSessionCount_++;
  UMA_HISTOGRAM_ENUMERATION(kPrerenderFinalStatusHistogramName,
                            PRERENDER_FINAL_STATUS_USED,
                            PRERENDER_FINAL_STATUS_MAX);
  [self removeScheduledPrerenderRequests];
  prerenderedURL_ = GURL();

  if (!webState_)
    return nullptr;

  // Move the pre-rendered WebState to a local variable so that it will no
  // longer be considered as pre-rendering (otherwise tab helpers may early
  // exist when invoked).
  std::unique_ptr<web::WebState> webState = std::move(webState_);
  DCHECK(![self isWebStatePrerendered:webState.get()]);

  Tab* tab = LegacyTabHelper::GetTabForWebState(webState.get());
  [[tab webController] setNativeProvider:nil];
  webState->SetShouldSuppressDialogs(false);
  webState->SetDelegate(nullptr);

  HistoryTabHelper::FromWebState(webState.get())
      ->SetDelayHistoryServiceNotification(false);

  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForBrowserState(
              browserState_)) {
    accountConsistencyService->RemoveWebStateHandler(webState.get());
  }

  if ([tab loadFinished]) {
    // If the page has finished loading, take a snapshot.  If the page is
    // still loading, do nothing, as CRWWebController will automatically take
    // a snapshot once the load completes.
    [tab updateSnapshotWithOverlay:YES visibleFrameOnly:YES];

    [[OmniboxGeolocationController sharedInstance] finishPageLoadForTab:tab
                                                            loadSuccess:YES];

    if (!webState->GetLastCommittedURL().SchemeIs(kChromeUIScheme)) {
      base::RecordAction(base::UserMetricsAction("MobilePageLoaded"));
    }
  }

  [tab setDelegate:nil];

  return webState;
}

- (void)connectionTypeChanged:(net::NetworkChangeNotifier::ConnectionType)type {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  usingWWAN_ = net::NetworkChangeNotifier::IsConnectionCellular(type);
  if (wifiOnly_ && usingWWAN_)
    [self cancelPrerender];
}

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kNetworkPredictionEnabled ||
      preferenceName == prefs::kNetworkPredictionWifiOnly) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    // The logic is simpler if both preferences changes are handled equally.
    enabled_ =
        browserState_->GetPrefs()->GetBoolean(prefs::kNetworkPredictionEnabled);
    wifiOnly_ = browserState_->GetPrefs()->GetBoolean(
        prefs::kNetworkPredictionWifiOnly);

    if (wifiOnly_ && enabled_) {
      if (!connectionTypeObserverBridge_.get()) {
        usingWWAN_ = net::NetworkChangeNotifier::IsConnectionCellular(
            net::NetworkChangeNotifier::GetConnectionType());
        connectionTypeObserverBridge_.reset(
            new ConnectionTypeObserverBridge(self));
      }
      if (usingWWAN_) {
        [self cancelPrerender];
      }
    } else if (enabled_) {
      connectionTypeObserverBridge_.reset();
    } else {
      [self cancelPrerender];
      connectionTypeObserverBridge_.reset();
    }
  }
}

- (void)didReceiveMemoryWarning {
  [self cancelPrerenderForReason:PRERENDER_FINAL_STATUS_MEMORY_LIMIT_EXCEEDED];
}

#pragma mark -
#pragma mark CRWNativeContentProvider implementation

- (BOOL)hasControllerForURL:(const GURL&)url {
  if (!webState_)
    return NO;

  return [delegate_ preloadHasNativeControllerForURL:url];
}

// Override the CRWNativeContentProvider methods to cancel any prerenders that
// require native content.
- (id<CRWNativeContent>)controllerForURL:(const GURL&)url
                                webState:(web::WebState*)webState {
  [self schedulePrerenderCancel];
  return nil;
}

// Override the CRWNativeContentProvider methods to cancel any prerenders that
// require native content.
- (id<CRWNativeContent>)controllerForURL:(const GURL&)url
                               withError:(NSError*)error
                                  isPost:(BOOL)isPost {
  [self schedulePrerenderCancel];
  return nil;
}

#pragma mark -
#pragma mark Private Methods

- (BOOL)isPrerenderingEnabled {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return !IsPrerenderTabEvictionExperimentalGroup() && enabled_ &&
         !ios::device_util::IsSingleCoreDevice() &&
         ios::device_util::RamIsAtLeast512Mb() && (!wifiOnly_ || !usingWWAN_);
}

- (BOOL)shouldPreloadURL:(const GURL&)url {
  return url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme);
}

- (void)startPrerender {
  // Destroy any existing prerenders before starting a new one.
  [self destroyPreviewContents];
  prerenderedURL_ = scheduledURL_;
  scheduledURL_ = GURL();

  DCHECK(prerenderedURL_.is_valid());
  if (!prerenderedURL_.is_valid()) {
    [self destroyPreviewContents];
    return;
  }

  web::WebState::CreateParams createParams(browserState_);
  webState_ = web::WebState::Create(createParams);
  AttachTabHelpers(webState_.get());

  Tab* tab = LegacyTabHelper::GetTabForWebState(webState_.get());
  DCHECK(tab);

  [[tab webController] setNativeProvider:self];
  webState_->SetDelegate(webStateDelegate_.get());
  webState_->SetShouldSuppressDialogs(true);
  webState_->SetWebUsageEnabled(true);
  [tab setDelegate:self];
  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForBrowserState(
              browserState_)) {
    accountConsistencyService->SetWebStateHandler(webState_.get(), self);
  }

  HistoryTabHelper::FromWebState(webState_.get())
      ->SetDelayHistoryServiceNotification(true);

  web::NavigationManager::WebLoadParams loadParams(prerenderedURL_);
  loadParams.referrer = scheduledReferrer_;
  loadParams.transition_type = scheduledTransition_;
  if ([delegate_ preloadShouldUseDesktopUserAgent]) {
    loadParams.user_agent_override_option =
        web::NavigationManager::UserAgentOverrideOption::DESKTOP;
  }
  webState_->GetNavigationManager()->LoadURLWithParams(loadParams);

  // Trigger the page to start loading.
  // TODO(crbug.com/705819): Remove this call.
  [tab view];
}

- (void)destroyPreviewContents {
  [self destroyPreviewContentsForReason:PRERENDER_FINAL_STATUS_CANCELLED];
}

- (void)destroyPreviewContentsForReason:(PrerenderFinalStatus)reason {
  if (!webState_)
    return;

  UMA_HISTOGRAM_ENUMERATION(kPrerenderFinalStatusHistogramName, reason,
                            PRERENDER_FINAL_STATUS_MAX);

  Tab* tab = LegacyTabHelper::GetTabForWebState(webState_.get());
  [[tab webController] setNativeProvider:nil];
  webState_->SetDelegate(nullptr);
  webState_.reset();

  prerenderedURL_ = GURL();
}

- (void)schedulePrerenderCancel {
  // TODO(crbug.com/228550): Instead of cancelling the prerender, should we mark
  // it as failed instead?  That way, subsequent prerender requests for the same
  // URL will not kick off new prerenders.
  [self removeScheduledPrerenderRequests];
  [self performSelector:@selector(cancelPrerender) withObject:nil afterDelay:0];
}

- (void)removeScheduledPrerenderRequests {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  scheduledURL_ = GURL();
}

#pragma mark - CRWWebStateDelegate

- (void)webState:(web::WebState*)webState
    didRequestHTTPAuthForProtectionSpace:(NSURLProtectionSpace*)protectionSpace
                      proposedCredential:(NSURLCredential*)proposedCredential
                       completionHandler:(void (^)(NSString* username,
                                                   NSString* password))handler {
  DCHECK([self isWebStatePrerendered:webState]);
  [self schedulePrerenderCancel];
  if (handler) {
    handler(nil, nil);
  }
}

#pragma mark - TabDelegate

- (void)discardPrerender {
  [self schedulePrerenderCancel];
}

#pragma mark - ManageAccountsDelegate

- (void)onManageAccounts {
  [self discardPrerender];
}

- (void)onAddAccount {
  [self discardPrerender];
}

- (void)onGoIncognito:(const GURL&)url {
  [self discardPrerender];
}

@end
