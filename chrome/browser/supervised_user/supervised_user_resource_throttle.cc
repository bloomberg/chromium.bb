// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_resource_throttle.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"
#include "ui/base/page_transition_types.h"

using content::BrowserThread;

namespace {

// These values corresponds to SupervisedUserSafetyFilterResult in
// tools/metrics/histograms/histograms.xml. If you change anything here, make
// sure to also update histograms.xml accordingly.
enum {
  FILTERING_BEHAVIOR_ALLOW = 1,
  FILTERING_BEHAVIOR_ALLOW_UNCERTAIN,
  FILTERING_BEHAVIOR_BLOCK_BLACKLIST,
  FILTERING_BEHAVIOR_BLOCK_SAFESITES,
  FILTERING_BEHAVIOR_MAX = FILTERING_BEHAVIOR_BLOCK_SAFESITES
};
const int kHistogramFilteringBehaviorSpacing = 100;
const int kHistogramPageTransitionMaxKnownValue =
    static_cast<int>(ui::PAGE_TRANSITION_KEYWORD_GENERATED);
const int kHistogramPageTransitionFallbackValue =
    kHistogramFilteringBehaviorSpacing - 1;
const int kHistogramMax = 500;

static_assert(kHistogramPageTransitionMaxKnownValue <
                  kHistogramPageTransitionFallbackValue,
              "HistogramPageTransition MaxKnownValue must be < FallbackValue");
static_assert(FILTERING_BEHAVIOR_MAX * kHistogramFilteringBehaviorSpacing +
                  kHistogramPageTransitionFallbackValue < kHistogramMax,
              "Invalid HistogramMax value");

int GetHistogramValueForFilteringBehavior(
    SupervisedUserURLFilter::FilteringBehavior behavior,
    SupervisedUserURLFilter::FilteringBehaviorSource source,
    bool uncertain) {
  // Since we're only interested in statistics about the blacklist and
  // SafeSites, count everything that got through to the default fallback
  // behavior as ALLOW (made it through all the filters!).
  if (source == SupervisedUserURLFilter::DEFAULT)
    behavior = SupervisedUserURLFilter::ALLOW;

  switch (behavior) {
    case SupervisedUserURLFilter::ALLOW:
      return uncertain ? FILTERING_BEHAVIOR_ALLOW_UNCERTAIN
                       : FILTERING_BEHAVIOR_ALLOW;
    case SupervisedUserURLFilter::BLOCK:
      if (source == SupervisedUserURLFilter::BLACKLIST)
        return FILTERING_BEHAVIOR_BLOCK_BLACKLIST;
      else if (source == SupervisedUserURLFilter::ASYNC_CHECKER)
        return FILTERING_BEHAVIOR_BLOCK_SAFESITES;
      // Fall through.
    default:
      NOTREACHED();
  }
  return 0;
}

int GetHistogramValueForTransitionType(ui::PageTransition transition_type) {
  int value =
      static_cast<int>(ui::PageTransitionStripQualifier(transition_type));
  if (0 <= value && value <= kHistogramPageTransitionMaxKnownValue)
    return value;
  NOTREACHED();
  return kHistogramPageTransitionFallbackValue;
}

void RecordFilterResultEvent(
    SupervisedUserURLFilter::FilteringBehavior behavior,
    SupervisedUserURLFilter::FilteringBehaviorSource source,
    bool uncertain,
    ui::PageTransition transition_type) {
  DCHECK(source != SupervisedUserURLFilter::MANUAL);
  int value =
      GetHistogramValueForFilteringBehavior(behavior, source, uncertain) *
          kHistogramFilteringBehaviorSpacing +
      GetHistogramValueForTransitionType(transition_type);
  DCHECK_LT(value, kHistogramMax);
  UMA_HISTOGRAM_ENUMERATION("ManagedUsers.SafetyFilter",
                            value, kHistogramMax);
}

}  // namespace

SupervisedUserResourceThrottle::SupervisedUserResourceThrottle(
    const net::URLRequest* request,
    bool is_main_frame,
    const SupervisedUserURLFilter* url_filter)
    : request_(request),
      is_main_frame_(is_main_frame),
      url_filter_(url_filter),
      weak_ptr_factory_(this) {}

SupervisedUserResourceThrottle::~SupervisedUserResourceThrottle() {}

void SupervisedUserResourceThrottle::ShowInterstitialIfNeeded(bool is_redirect,
                                                              const GURL& url,
                                                              bool* defer) {
  // Only treat main frame requests for now (ignoring subresources).
  if (!is_main_frame_)
    return;

  deferred_ = false;
  bool got_result = url_filter_->GetFilteringBehaviorForURLWithAsyncChecks(
      url,
      base::Bind(&SupervisedUserResourceThrottle::OnCheckDone,
                 weak_ptr_factory_.GetWeakPtr(), url));
  // If we got a "not blocked" result synchronously, don't defer.
  if (got_result && behavior_ != SupervisedUserURLFilter::BLOCK)
    return;

  *defer = true;
  deferred_ = true;
}

void SupervisedUserResourceThrottle::ShowInterstitial(const GURL& url) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&SupervisedUserNavigationObserver::OnRequestBlocked,
                 info->GetChildID(), info->GetRouteID(), url,
                 base::Bind(
                     &SupervisedUserResourceThrottle::OnInterstitialResult,
                     weak_ptr_factory_.GetWeakPtr())));
}

void SupervisedUserResourceThrottle::WillStartRequest(bool* defer) {
  ShowInterstitialIfNeeded(false, request_->url(), defer);
}

void SupervisedUserResourceThrottle::WillRedirectRequest(const GURL& new_url,
                                                         bool* defer) {
  ShowInterstitialIfNeeded(true, new_url, defer);
}

const char* SupervisedUserResourceThrottle::GetNameForLogging() const {
  return "SupervisedUserResourceThrottle";
}

void SupervisedUserResourceThrottle::OnCheckDone(
    const GURL& url,
    SupervisedUserURLFilter::FilteringBehavior behavior,
    SupervisedUserURLFilter::FilteringBehaviorSource source,
    bool uncertain) {
  behavior_ = behavior;

  // If both the static blacklist and SafeSites are enabled, record UMA events.
  if (url_filter_->HasBlacklist() && url_filter_->HasAsyncURLChecker() &&
      source < SupervisedUserURLFilter::MANUAL) {
    const content::ResourceRequestInfo* info =
        content::ResourceRequestInfo::ForRequest(request_);
    RecordFilterResultEvent(behavior, source, uncertain,
                            info->GetPageTransition());
  }

  if (behavior == SupervisedUserURLFilter::BLOCK) {
    ShowInterstitial(url);
  } else if (deferred_) {
    controller()->Resume();
  }
}

void SupervisedUserResourceThrottle::OnInterstitialResult(
    bool continue_request) {
  if (continue_request)
    controller()->Resume();
  else
    controller()->Cancel();
}
