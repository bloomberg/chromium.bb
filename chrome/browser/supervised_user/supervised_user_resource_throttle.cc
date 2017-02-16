// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_resource_throttle.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/redirect_info.h"
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
  FILTERING_BEHAVIOR_BLOCK_MANUAL,
  FILTERING_BEHAVIOR_BLOCK_DEFAULT,
  FILTERING_BEHAVIOR_ALLOW_WHITELIST,
  FILTERING_BEHAVIOR_MAX = FILTERING_BEHAVIOR_ALLOW_WHITELIST
};
const int kHistogramFilteringBehaviorSpacing = 100;
const int kHistogramPageTransitionMaxKnownValue =
    static_cast<int>(ui::PAGE_TRANSITION_KEYWORD_GENERATED);
const int kHistogramPageTransitionFallbackValue =
    kHistogramFilteringBehaviorSpacing - 1;
const int kHistogramMax = 800;

static_assert(kHistogramPageTransitionMaxKnownValue <
                  kHistogramPageTransitionFallbackValue,
              "HistogramPageTransition MaxKnownValue must be < FallbackValue");
static_assert(FILTERING_BEHAVIOR_MAX * kHistogramFilteringBehaviorSpacing +
                  kHistogramPageTransitionFallbackValue < kHistogramMax,
              "Invalid HistogramMax value");

int GetHistogramValueForFilteringBehavior(
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool uncertain) {
  switch (behavior) {
    case SupervisedUserURLFilter::ALLOW:
    case SupervisedUserURLFilter::WARN:
      if (reason == supervised_user_error_page::WHITELIST)
        return FILTERING_BEHAVIOR_ALLOW_WHITELIST;
      return uncertain ? FILTERING_BEHAVIOR_ALLOW_UNCERTAIN
                       : FILTERING_BEHAVIOR_ALLOW;
    case SupervisedUserURLFilter::BLOCK:
      switch (reason) {
        case supervised_user_error_page::BLACKLIST:
          return FILTERING_BEHAVIOR_BLOCK_BLACKLIST;
        case supervised_user_error_page::ASYNC_CHECKER:
          return FILTERING_BEHAVIOR_BLOCK_SAFESITES;
        case supervised_user_error_page::WHITELIST:
          NOTREACHED();
          break;
        case supervised_user_error_page::MANUAL:
          return FILTERING_BEHAVIOR_BLOCK_MANUAL;
        case supervised_user_error_page::DEFAULT:
          return FILTERING_BEHAVIOR_BLOCK_DEFAULT;
        case supervised_user_error_page::NOT_SIGNED_IN:
          // Should never happen, only used for requests from Webview
          NOTREACHED();
      }
    case SupervisedUserURLFilter::INVALID:
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
    bool safesites_histogram,
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool uncertain,
    ui::PageTransition transition_type) {
  int value =
      GetHistogramValueForFilteringBehavior(behavior, reason, uncertain) *
          kHistogramFilteringBehaviorSpacing +
      GetHistogramValueForTransitionType(transition_type);
  DCHECK_LT(value, kHistogramMax);
  // Note: We can't pass in the histogram name as a parameter to this function
  // because of how the macro works (look up the histogram on the first
  // invocation and cache it in a static variable).
  if (safesites_histogram)
    UMA_HISTOGRAM_SPARSE_SLOWLY("ManagedUsers.SafetyFilter", value);
  else
    UMA_HISTOGRAM_SPARSE_SLOWLY("ManagedUsers.FilteringResult", value);
}

// Helper function to wrap a given callback in one that will post it to the
// IO thread.
base::Callback<void(bool)> ResultTrampoline(
    const base::Callback<void(bool)>& callback) {
  return base::Bind(
      [](const base::Callback<void(bool)>& callback, bool result) {
        BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                                base::Bind(callback, result));
      },
      callback);
}

}  // namespace

// static
std::unique_ptr<SupervisedUserResourceThrottle>
SupervisedUserResourceThrottle::MaybeCreate(
    const net::URLRequest* request,
    content::ResourceType resource_type,
    const SupervisedUserURLFilter* url_filter) {
  // Only treat main frame requests (ignoring subframes and subresources).
  bool is_main_frame = resource_type == content::RESOURCE_TYPE_MAIN_FRAME;
  if (!is_main_frame)
    return nullptr;

  // Can't use base::MakeUnique because the constructor is private.
  return base::WrapUnique(
      new SupervisedUserResourceThrottle(request, url_filter));
}

SupervisedUserResourceThrottle::SupervisedUserResourceThrottle(
    const net::URLRequest* request,
    const SupervisedUserURLFilter* url_filter)
    : request_(request),
      url_filter_(url_filter),
      deferred_(false),
      behavior_(SupervisedUserURLFilter::INVALID),
      weak_ptr_factory_(this) {}

SupervisedUserResourceThrottle::~SupervisedUserResourceThrottle() {}

void SupervisedUserResourceThrottle::CheckURL(const GURL& url, bool* defer) {
  deferred_ = false;
  DCHECK_EQ(SupervisedUserURLFilter::INVALID, behavior_);
  bool got_result = url_filter_->GetFilteringBehaviorForURLWithAsyncChecks(
      url,
      base::Bind(&SupervisedUserResourceThrottle::OnCheckDone,
                 weak_ptr_factory_.GetWeakPtr(), url));
  DCHECK_EQ(got_result, behavior_ != SupervisedUserURLFilter::INVALID);
  // If we got a "not blocked" result synchronously, don't defer.
  *defer = deferred_ = !got_result ||
                       (behavior_ == SupervisedUserURLFilter::BLOCK);
  if (got_result)
    behavior_ = SupervisedUserURLFilter::INVALID;
}

void SupervisedUserResourceThrottle::ShowInterstitial(
    const GURL& url,
    supervised_user_error_page::FilteringBehaviorReason reason) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SupervisedUserNavigationObserver::OnRequestBlocked,
                 info->GetWebContentsGetterForRequest(), url, reason,
                 ResultTrampoline(base::Bind(
                     &SupervisedUserResourceThrottle::OnInterstitialResult,
                     weak_ptr_factory_.GetWeakPtr()))));
}

void SupervisedUserResourceThrottle::WillStartRequest(bool* defer) {
  CheckURL(request_->url(), defer);
}

void SupervisedUserResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  CheckURL(redirect_info.new_url, defer);
}

const char* SupervisedUserResourceThrottle::GetNameForLogging() const {
  return "SupervisedUserResourceThrottle";
}

void SupervisedUserResourceThrottle::OnCheckDone(
    const GURL& url,
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool uncertain) {
  DCHECK_EQ(SupervisedUserURLFilter::INVALID, behavior_);
  // If we got a result synchronously, pass it back to ShowInterstitialIfNeeded.
  if (!deferred_)
    behavior_ = behavior;

  ui::PageTransition transition =
      content::ResourceRequestInfo::ForRequest(request_)->GetPageTransition();

  RecordFilterResultEvent(false, behavior, reason, uncertain, transition);

  // If both the static blacklist and the async checker are enabled, also record
  // SafeSites-only UMA events.
  if (url_filter_->HasBlacklist() && url_filter_->HasAsyncURLChecker() &&
      (reason == supervised_user_error_page::ASYNC_CHECKER ||
       reason == supervised_user_error_page::BLACKLIST)) {
    RecordFilterResultEvent(true, behavior, reason, uncertain, transition);
  }

  if (behavior == SupervisedUserURLFilter::BLOCK)
    ShowInterstitial(url, reason);
  else if (deferred_)
    Resume();
}

void SupervisedUserResourceThrottle::OnInterstitialResult(
    bool continue_request) {
  if (continue_request)
    Resume();
  else
    Cancel();
}
