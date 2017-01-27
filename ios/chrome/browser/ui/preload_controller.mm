// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/preload_controller.h"

#include "base/ios/device_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/tabs/tab.h"
#include "ios/chrome/browser/ui/preload_controller_delegate.h"
#include "ios/chrome/browser/ui/prerender_final_status.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "net/base/mac/url_conversions.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "ui/base/page_transition_types.h"

// ID of the URLFetcher responsible for prefetches.
const int kPreloadControllerURLFetcherID = 1;

namespace {
// Delay before starting to prerender a URL.
const NSTimeInterval kPrerenderDelay = 0.5;

// The finch experiment to turn off prefetching as a field trial.
const char kTabEvictionFieldTrialName[] = "TabEviction";
// The associated group.
const char kPrerenderTabEvictionTrialGroup[] = "NoPrerendering";
// The name of the histogram for recording final status (e.g. used/cancelled)
// of prerender requests.
const char kPrerenderFinalStatusHistogramName[] = "Prerender.FinalStatus";
// The name of the histogram for recording the number of sucessful prerenders.
const char kPrerendersPerSessionCountHistogramName[] =
    "Prerender.PrerendersPerSessionCount";

// Is this install selected for this particular experiment.
bool IsPrerenderTabEvictionExperimentalGroup() {
  base::FieldTrial* trial =
      base::FieldTrialList::Find(kTabEvictionFieldTrialName);
  return trial && trial->group_name() == kPrerenderTabEvictionTrialGroup;
}

}  // namespace

@interface PreloadController (PrivateMethods)

// Returns YES if prerendering is enabled.
- (BOOL)isPrerenderingEnabled;

// Returns YES if prefetching is enabled.
- (BOOL)isPrefetchingEnabled;

// Returns YES if the |url| is valid for prerendering and prefetching.
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

// Starts the scheduled prefetch request.
- (void)startPrefetch;

// Cancels the scheduled prefetch request.
- (void)cancelPrefetch;

// Completes the current prefetch request. Called by the URLFetcher's delegate
// when the URLFetcher has compleeted fetching the |prefetchedURL|.
- (void)prefetchDidComplete:(const net::URLFetcher*)source;

@end

// Delegate to handle completion of URLFetcher operations.
class PrefetchDelegate : public net::URLFetcherDelegate {
 public:
  explicit PrefetchDelegate(PreloadController* owner) : owner_(owner) {}
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    [owner_ prefetchDidComplete:source];
  }

 private:
  PreloadController* owner_;  // weak
};

@implementation PreloadController

@synthesize prerenderedURL = prerenderedURL_;
@synthesize prefetchedURL = prefetchedURL_;
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
  [super dealloc];
}

- (void)prerenderURL:(const GURL&)url
            referrer:(const web::Referrer&)referrer
          transition:(ui::PageTransition)transition
         immediately:(BOOL)immediately {
  // TODO(rohitrao): If shouldPrerenderURL returns false, should we cancel any
  // scheduled prerender requests?
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

- (void)prefetchURL:(const GURL&)url transition:(ui::PageTransition)transition {
  if (![self isPrefetchingEnabled] || ![self shouldPreloadURL:url]) {
    return;
  }

  // Ignore this request if the the currently prefetched page matches this URL.
  if ([self hasPrefetchedURL:url]) {
    return;
  }

  // Cancel any in-fight prefetches before starting a new one.
  [self cancelPrefetch];

  DCHECK(url.is_valid());
  if (!url.is_valid()) {
    return;
  }

  prefetchedURL_ = [self urlToPrefetchURL:url];
  prefetcherDelegate_.reset(new PrefetchDelegate(self));
  prefetcher_ =
      net::URLFetcher::Create(kPreloadControllerURLFetcherID, prefetchedURL_,
                              net::URLFetcher::GET, prefetcherDelegate_.get());
  prefetcher_->SetRequestContext(browserState_->GetRequestContext());
  prefetcher_->Start();
}

- (void)cancelPrerender {
  [self cancelPrerenderForReason:PRERENDER_FINAL_STATUS_CANCELLED];
}

- (void)cancelPrerenderForReason:(PrerenderFinalStatus)reason {
  [self removeScheduledPrerenderRequests];
  [self destroyPreviewContentsForReason:reason];
}

- (Tab*)releasePrerenderContents {
  successfulPrerendersPerSessionCount_++;
  UMA_HISTOGRAM_ENUMERATION(kPrerenderFinalStatusHistogramName,
                            PRERENDER_FINAL_STATUS_USED,
                            PRERENDER_FINAL_STATUS_MAX);
  [self removeScheduledPrerenderRequests];
  prerenderedURL_ = GURL();
  [[tab_ webController] setNativeProvider:nil];
  [tab_ setDelegate:nil];
  return [tab_.release() autorelease];
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

// Delegate the call to the original native provider.
- (BOOL)hasControllerForURL:(const GURL&)url {
  return [[tab_ webController].nativeProvider hasControllerForURL:url];
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

- (BOOL)isPrefetchingEnabled {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return enabled_ && (!wifiOnly_ || !usingWWAN_);
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

  Tab* tab = [Tab
      newPreloadingTabWithBrowserState:browserState_
                                   url:prerenderedURL_
                              referrer:scheduledReferrer_
                            transition:scheduledTransition_
                              provider:self
                                opener:nil
                      desktopUserAgent:[delegate_ shouldUseDesktopUserAgent]
                         configuration:^(Tab* tab) {
                           [tab setIsPrerenderTab:YES];
                           [tab setDelegate:self];
                         }];

  // Create and set up the prerender.
  tab_.reset([tab retain]);

  // Trigger the page to start loading.
  [tab_ view];
}

- (const GURL)urlToPrefetchURL:(const GURL&)url {
  GURL::Replacements replacements;

  // Add prefetch indicator to query params.
  std::string query = url.query();
  if (!query.empty()) {
    query.append("&");
  }
  query.append("pf=i");
  replacements.SetQueryStr(query);

  return url.ReplaceComponents(replacements);
}

- (BOOL)hasPrefetchedURL:(const GURL&)url {
  return prefetchedURL_ == [self urlToPrefetchURL:url];
}

- (void)cancelPrefetch {
  prefetcher_.reset();
  prefetchedURL_ = GURL();
}

- (void)prefetchDidComplete:(const net::URLFetcher*)source {
  if (source) {
    DLOG_IF(WARNING, source->GetResponseCode() != 200)
        << "Prefetching URL got response code " << source->GetResponseCode();
  }
  prefetcher_.reset();
}

- (void)destroyPreviewContents {
  [self destroyPreviewContentsForReason:PRERENDER_FINAL_STATUS_CANCELLED];
}

- (void)destroyPreviewContentsForReason:(PrerenderFinalStatus)reason {
  if (!tab_.get())
    return;

  UMA_HISTOGRAM_ENUMERATION(kPrerenderFinalStatusHistogramName, reason,
                            PRERENDER_FINAL_STATUS_MAX);
  [[tab_ webController] setNativeProvider:nil];
  [tab_ setDelegate:nil];
  [tab_ close];
  tab_.reset();
  prerenderedURL_ = GURL();
}

- (void)schedulePrerenderCancel {
  // TODO(rohitrao): Instead of cancelling the prerender, should we mark it as
  // failed instead?  That way, subsequent prerender requests for the same URL
  // will not kick off new prerenders.  b/5944421
  [self removeScheduledPrerenderRequests];
  [self performSelector:@selector(cancelPrerender) withObject:nil afterDelay:0];
}

- (void)removeScheduledPrerenderRequests {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  scheduledURL_ = GURL();
}

#pragma mark - TabDelegate

- (void)discardPrerender {
  [self schedulePrerenderCancel];
}

@end
