// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/browser/content_activation_list_utils.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    subresource_filter::ContentSubresourceFilterDriverFactory);

namespace subresource_filter {

namespace {

std::string DistillURLToHostAndPath(const GURL& url) {
  return url.host() + url.path();
}

// Returns true with a probability given by |performance_measurement_rate| if
// ThreadTicks is supported, otherwise returns false.
bool ShouldMeasurePerformanceForPageLoad(double performance_measurement_rate) {
  if (!base::ThreadTicks::IsSupported())
    return false;
  return performance_measurement_rate == 1 ||
         (performance_measurement_rate > 0 &&
          base::RandDouble() < performance_measurement_rate);
}

// Records histograms about the length of redirect chains, and about the pattern
// of whether each URL in the chain matched the activation list.
#define REPORT_REDIRECT_PATTERN_FOR_SUFFIX(suffix, hits_pattern, chain_size)   \
  do {                                                                         \
    UMA_HISTOGRAM_ENUMERATION(                                                 \
        "SubresourceFilter.PageLoad.RedirectChainMatchPattern." suffix,        \
        hits_pattern, 0x10);                                                   \
    UMA_HISTOGRAM_COUNTS(                                                      \
        "SubresourceFilter.PageLoad.RedirectChainLength." suffix, chain_size); \
  } while (0)

}  // namespace

// static
void ContentSubresourceFilterDriverFactory::CreateForWebContents(
    content::WebContents* web_contents,
    SubresourceFilterClient* client) {
  if (FromWebContents(web_contents))
    return;
  web_contents->SetUserData(
      UserDataKey(), base::MakeUnique<ContentSubresourceFilterDriverFactory>(
                         web_contents, client));
}

// static
bool ContentSubresourceFilterDriverFactory::NavigationIsPageReload(
    const GURL& url,
    const content::Referrer& referrer,
    ui::PageTransition transition) {
  return ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD) ||
         // Some pages 'reload' from JavaScript by navigating to themselves.
         url == referrer.url;
}

ContentSubresourceFilterDriverFactory::ContentSubresourceFilterDriverFactory(
    content::WebContents* web_contents,
    SubresourceFilterClient* client)
    : content::WebContentsObserver(web_contents),
      client_(client),
      throttle_manager_(
          base::MakeUnique<ContentSubresourceFilterThrottleManager>(
              this,
              client_->GetRulesetDealer(),
              web_contents)),
      activation_level_(ActivationLevel::DISABLED),
      activation_decision_(ActivationDecision::UNKNOWN),
      measure_performance_(false) {}

ContentSubresourceFilterDriverFactory::
    ~ContentSubresourceFilterDriverFactory() {}

void ContentSubresourceFilterDriverFactory::
    OnMainResourceMatchedSafeBrowsingBlacklist(
        const GURL& url,
        const std::vector<GURL>& redirect_urls,
        safe_browsing::SBThreatType threat_type,
        safe_browsing::ThreatPatternType threat_type_metadata) {
  AddActivationListMatch(
      url, GetListForThreatTypeAndMetadata(threat_type, threat_type_metadata));
}

ContentSubresourceFilterDriverFactory::ActivationDecision
ContentSubresourceFilterDriverFactory::
    ComputeActivationDecisionForMainFrameNavigation(
        content::NavigationHandle* navigation_handle) const {
  const GURL& url(navigation_handle->GetURL());

  const auto configurations = GetActiveConfigurations();
  if (configurations->the_one_and_only().activation_level ==
      ActivationLevel::DISABLED)
    return ActivationDecision::ACTIVATION_DISABLED;

  if (configurations->the_one_and_only().activation_scope ==
      ActivationScope::NO_SITES)
    return ActivationDecision::ACTIVATION_DISABLED;

  if (!url.SchemeIsHTTPOrHTTPS())
    return ActivationDecision::UNSUPPORTED_SCHEME;
  // TODO(csharrison): The throttle manager also performs this check. Remove
  // this one when the activation decision is sent directly to the throttle
  // manager.
  if (client_->ShouldSuppressActivation(navigation_handle))
    return ActivationDecision::URL_WHITELISTED;

  switch (configurations->the_one_and_only().activation_scope) {
    case ActivationScope::ALL_SITES:
      return ActivationDecision::ACTIVATED;
    case ActivationScope::ACTIVATION_LIST: {
      // The logic to ensure only http/https URLs are activated lives in
      // AddActivationListMatch to ensure the activation list only has relevant
      // entries.
      DCHECK(url.SchemeIsHTTPOrHTTPS() ||
             !DidURLMatchActivationList(
                 url, configurations->the_one_and_only().activation_list));
      bool should_activate = DidURLMatchActivationList(
          url, configurations->the_one_and_only().activation_list);
      if (configurations->the_one_and_only().activation_list ==
          ActivationList::PHISHING_INTERSTITIAL) {
        // Handling special case, where activation on the phishing sites also
        // mean the activation on the sites with social engineering metadata.
        should_activate |= DidURLMatchActivationList(
            url, ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL);
      }
      return should_activate ? ActivationDecision::ACTIVATED
                             : ActivationDecision::ACTIVATION_LIST_NOT_MATCHED;
    }
    default:
      return ActivationDecision::ACTIVATION_DISABLED;
  }
}

void ContentSubresourceFilterDriverFactory::OnReloadRequested() {
  UMA_HISTOGRAM_BOOLEAN("SubresourceFilter.Prompt.NumReloads", true);
  const GURL& whitelist_url = web_contents()->GetLastCommittedURL();

  // Only whitelist via content settings when using the experimental UI,
  // otherwise could get into a situation where content settings cannot be
  // adjusted.
  if (base::FeatureList::IsEnabled(
          subresource_filter::kSafeBrowsingSubresourceFilterExperimentalUI)) {
    client_->WhitelistByContentSettings(whitelist_url);
  } else {
    client_->WhitelistInCurrentWebContents(whitelist_url);
  }
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

void ContentSubresourceFilterDriverFactory::WillProcessResponse(
    content::NavigationHandle* navigation_handle) {
  DCHECK(!navigation_handle->IsSameDocument());
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->GetNetErrorCode() != net::OK) {
    return;
  }

  const GURL& url = navigation_handle->GetURL();
  const content::Referrer& referrer = navigation_handle->GetReferrer();
  ui::PageTransition transition = navigation_handle->GetPageTransition();

  RecordRedirectChainMatchPattern();

  const auto configurations = GetActiveConfigurations();
  if (configurations->the_one_and_only().should_whitelist_site_on_reload &&
      NavigationIsPageReload(url, referrer, transition)) {
    // Whitelist this host for the current as well as subsequent navigations.
    client_->WhitelistInCurrentWebContents(url);
  }

  activation_decision_ =
      ComputeActivationDecisionForMainFrameNavigation(navigation_handle);
  DCHECK(activation_decision_ != ActivationDecision::UNKNOWN);
  if (activation_decision_ != ActivationDecision::ACTIVATED) {
    ResetActivationState();
    return;
  }

  activation_level_ = configurations->the_one_and_only().activation_level;
  measure_performance_ =
      activation_level_ != ActivationLevel::DISABLED &&
      ShouldMeasurePerformanceForPageLoad(
          configurations->the_one_and_only().performance_measurement_rate);
  ActivationState state = ActivationState(activation_level_);
  state.measure_performance = measure_performance_;
  // TODO(csharrison): Set state.enable_logging based on metadata returns from
  // the safe browsing filter, when it is available. Add tests for this
  // behavior.
  throttle_manager_->NotifyPageActivationComputed(navigation_handle, state);
}

void ContentSubresourceFilterDriverFactory::OnFirstSubresourceLoadDisallowed() {
  const auto configurations = GetActiveConfigurations();
  if (configurations->the_one_and_only().should_suppress_notifications)
    return;

  client_->ToggleNotificationVisibility(activation_level_ ==
                                        ActivationLevel::ENABLED);
}

bool ContentSubresourceFilterDriverFactory::ShouldSuppressActivation(
    content::NavigationHandle* navigation_handle) {
  return client_->ShouldSuppressActivation(navigation_handle);
}

void ContentSubresourceFilterDriverFactory::ResetActivationState() {
  navigation_chain_.clear();
  activation_list_matches_.clear();
  activation_level_ = ActivationLevel::DISABLED;
  measure_performance_ = false;
}

void ContentSubresourceFilterDriverFactory::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    activation_decision_ = ActivationDecision::UNKNOWN;
    ResetActivationState();
    navigation_chain_.push_back(navigation_handle->GetURL());
    client_->ToggleNotificationVisibility(false);
  }
}

void ContentSubresourceFilterDriverFactory::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(!navigation_handle->IsSameDocument());
  if (navigation_handle->IsInMainFrame())
    navigation_chain_.push_back(navigation_handle->GetURL());
}

bool ContentSubresourceFilterDriverFactory::DidURLMatchActivationList(
    const GURL& url,
    ActivationList activation_list) const {
  auto match_types =
      activation_list_matches_.find(DistillURLToHostAndPath(url));
  return match_types != activation_list_matches_.end() &&
         match_types->second.find(activation_list) != match_types->second.end();
}

void ContentSubresourceFilterDriverFactory::AddActivationListMatch(
    const GURL& url,
    ActivationList match_type) {
  if (match_type == ActivationList::NONE)
    return;
  if (url.has_host() && url.SchemeIsHTTPOrHTTPS())
    activation_list_matches_[DistillURLToHostAndPath(url)].insert(match_type);
}

int ContentSubresourceFilterDriverFactory::CalculateHitPatternForActivationList(
    ActivationList activation_list) const {
  int hits_pattern = 0;
  const int kInitialURLHitMask = 0x4;
  const int kRedirectURLHitMask = 0x2;
  const int kFinalURLHitMask = 0x1;

  if (navigation_chain_.size() > 1) {
    if (DidURLMatchActivationList(navigation_chain_.back(), activation_list))
      hits_pattern |= kFinalURLHitMask;
    if (DidURLMatchActivationList(navigation_chain_.front(), activation_list))
      hits_pattern |= kInitialURLHitMask;

    // Examine redirects.
    for (size_t i = 1; i < navigation_chain_.size() - 1; ++i) {
      if (DidURLMatchActivationList(navigation_chain_[i], activation_list)) {
        hits_pattern |= kRedirectURLHitMask;
        break;
      }
    }
  } else {
    if (navigation_chain_.size() &&
        DidURLMatchActivationList(navigation_chain_.front(), activation_list)) {
      hits_pattern = 0x8;  // One url hit.
    }
  }
  return hits_pattern;
}

void ContentSubresourceFilterDriverFactory::RecordRedirectChainMatchPattern()
    const {
  RecordRedirectChainMatchPatternForList(
      ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL);
  RecordRedirectChainMatchPatternForList(ActivationList::PHISHING_INTERSTITIAL);
  RecordRedirectChainMatchPatternForList(ActivationList::SUBRESOURCE_FILTER);
}

void ContentSubresourceFilterDriverFactory::
    RecordRedirectChainMatchPatternForList(
        ActivationList activation_list) const {
  int hits_pattern = CalculateHitPatternForActivationList(activation_list);
  if (!hits_pattern)
    return;
  size_t chain_size = navigation_chain_.size();
  switch (activation_list) {
    case ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL:
      REPORT_REDIRECT_PATTERN_FOR_SUFFIX("SocialEngineeringAdsInterstitial",
                                         hits_pattern, chain_size);
      break;
    case ActivationList::PHISHING_INTERSTITIAL:
      REPORT_REDIRECT_PATTERN_FOR_SUFFIX("PhishingInterstital", hits_pattern,
                                         chain_size);
      break;
    case ActivationList::SUBRESOURCE_FILTER:
      REPORT_REDIRECT_PATTERN_FOR_SUFFIX("SubresourceFilterOnly", hits_pattern,
                                         chain_size);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace subresource_filter
