// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/core/browser/subresource_filter_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

const char kWebContentsUserDataKey[] =
    "web_contents_subresource_filter_driver_factory";

std::string DistillURLToHostAndPath(const GURL& url) {
  return url.host() + url.path();
}

// Returns true with a probability of GetPerformanceMeasurementRate() if
// ThreadTicks is supported, otherwise returns false.
bool ShouldMeasurePerformanceForPageLoad() {
  if (!base::ThreadTicks::IsSupported())
    return false;
  // TODO(pkalinnikov): Cache |rate| and other variation params in
  // ContentSubresourceFilterDriverFactory.
  const double rate = GetPerformanceMeasurementRate();
  return rate == 1 || (rate > 0 && base::RandDouble() < rate);
}

}  // namespace

// static
void ContentSubresourceFilterDriverFactory::CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<SubresourceFilterClient> client) {
  if (FromWebContents(web_contents))
    return;
  web_contents->SetUserData(kWebContentsUserDataKey,
                            new ContentSubresourceFilterDriverFactory(
                                web_contents, std::move(client)));
}

// static
ContentSubresourceFilterDriverFactory*
ContentSubresourceFilterDriverFactory::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<ContentSubresourceFilterDriverFactory*>(
      web_contents->GetUserData(kWebContentsUserDataKey));
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
    std::unique_ptr<SubresourceFilterClient> client)
    : content::WebContentsObserver(web_contents),
      client_(std::move(client)),
      activation_level_(ActivationLevel::DISABLED),
      activation_decision_(ActivationDecision::UNKNOWN),
      measure_performance_(false) {}

ContentSubresourceFilterDriverFactory::
    ~ContentSubresourceFilterDriverFactory() {}

void ContentSubresourceFilterDriverFactory::OnFirstSubresourceLoadDisallowed() {
  if (ShouldSuppressNotifications())
    return;

  client_->ToggleNotificationVisibility(activation_level_ ==
                                        ActivationLevel::ENABLED);
}

void ContentSubresourceFilterDriverFactory::OnDocumentLoadStatistics(
    const DocumentLoadStatistics& statistics) {
  // Note: Chances of overflow are negligible.
  aggregated_document_statistics_.num_loads_total += statistics.num_loads_total;
  aggregated_document_statistics_.num_loads_evaluated +=
      statistics.num_loads_evaluated;
  aggregated_document_statistics_.num_loads_matching_rules +=
      statistics.num_loads_matching_rules;
  aggregated_document_statistics_.num_loads_disallowed +=
      statistics.num_loads_disallowed;

  aggregated_document_statistics_.evaluation_total_wall_duration +=
      statistics.evaluation_total_wall_duration;
  aggregated_document_statistics_.evaluation_total_cpu_duration +=
      statistics.evaluation_total_cpu_duration;
}

bool ContentSubresourceFilterDriverFactory::IsWhitelisted(
    const GURL& url) const {
  return whitelisted_hosts_.find(url.host()) != whitelisted_hosts_.end();
}

void ContentSubresourceFilterDriverFactory::
    OnMainResourceMatchedSafeBrowsingBlacklist(
        const GURL& url,
        const std::vector<GURL>& redirect_urls,
        safe_browsing::SBThreatType threat_type,
        safe_browsing::ThreatPatternType threat_type_metadata) {
  bool is_phishing_interstitial =
      (threat_type == safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
  bool is_soc_engineering_ads_interstitial =
      threat_type_metadata ==
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS;

  if (is_phishing_interstitial) {
    if (is_soc_engineering_ads_interstitial) {
      AddActivationListMatch(url, ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL);
    }
    AddActivationListMatch(url, ActivationList::PHISHING_INTERSTITIAL);
  }
}

void ContentSubresourceFilterDriverFactory::AddHostOfURLToWhitelistSet(
    const GURL& url) {
  if (url.has_host() && url.SchemeIsHTTPOrHTTPS())
    whitelisted_hosts_.insert(url.host());
}

ContentSubresourceFilterDriverFactory::ActivationDecision
ContentSubresourceFilterDriverFactory::ComputeActivationDecisionForMainFrameURL(
    const GURL& url) const {
  if (GetMaximumActivationLevel() == ActivationLevel::DISABLED)
    return ActivationDecision::ACTIVATION_DISABLED;

  ActivationScope scope = GetCurrentActivationScope();
  if (scope == ActivationScope::NO_SITES)
    return ActivationDecision::ACTIVATION_DISABLED;

  if (!url.SchemeIsHTTPOrHTTPS())
    return ActivationDecision::UNSUPPORTED_SCHEME;
  if (IsWhitelisted(url))
    return ActivationDecision::URL_WHITELISTED;

  switch (scope) {
    case ActivationScope::ALL_SITES:
      return ActivationDecision::ACTIVATED;
    case ActivationScope::ACTIVATION_LIST:
      // The logic to ensure only http/https URLs are activated lives in
      // AddActivationListMatch to ensure the activation list only has relevant
      // entries.
      DCHECK(url.SchemeIsHTTPOrHTTPS() ||
             !DidURLMatchCurrentActivationList(url));
      return DidURLMatchCurrentActivationList(url)
                 ? ActivationDecision::ACTIVATED
                 : ActivationDecision::ACTIVATION_LIST_NOT_MATCHED;
    default:
      return ActivationDecision::ACTIVATION_DISABLED;
  }
}

void ContentSubresourceFilterDriverFactory::ActivateForFrameHostIfNeeded(
    content::RenderFrameHost* render_frame_host,
    const GURL& url) {
  if (activation_level_ != ActivationLevel::DISABLED) {
    render_frame_host->Send(
        new SubresourceFilterMsg_ActivateForNextCommittedLoad(
            render_frame_host->GetRoutingID(), activation_level_,
            measure_performance_));
  }
}

void ContentSubresourceFilterDriverFactory::OnReloadRequested() {
  UMA_HISTOGRAM_BOOLEAN("SubresourceFilter.Prompt.NumReloads", true);
  const GURL& whitelist_url = web_contents()->GetLastCommittedURL();
  AddHostOfURLToWhitelistSet(whitelist_url);
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

void ContentSubresourceFilterDriverFactory::ResetActivationState() {
  navigation_chain_.clear();
  activation_list_matches_.clear();
  activation_level_ = ActivationLevel::DISABLED;
  measure_performance_ = false;
  aggregated_document_statistics_ = DocumentLoadStatistics();
}

void ContentSubresourceFilterDriverFactory::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() && !navigation_handle->IsSamePage()) {
    activation_decision_ = ActivationDecision::UNKNOWN;
    ResetActivationState();
    navigation_chain_.push_back(navigation_handle->GetURL());
    client_->ToggleNotificationVisibility(false);
  }
}

void ContentSubresourceFilterDriverFactory::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(!navigation_handle->IsSamePage());
  if (navigation_handle->IsInMainFrame())
    navigation_chain_.push_back(navigation_handle->GetURL());
}

void ContentSubresourceFilterDriverFactory::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(!navigation_handle->IsSamePage());

  // ReadyToCommitNavigation with browser-side navigation disabled is not called
  // in production code for failed navigations (e.g. network errors). We don't
  // want to activate on these pages, so we bail early to guarantee consistent
  // behavior regardless of whether browser-side navigation is enabled.
  if (navigation_handle->GetNetErrorCode() != net::OK)
    return;

  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  GURL url = navigation_handle->GetURL();
  const content::Referrer& referrer = navigation_handle->GetReferrer();
  ui::PageTransition transition = navigation_handle->GetPageTransition();
  ReadyToCommitNavigationInternal(render_frame_host, url, referrer, transition);
}

void ContentSubresourceFilterDriverFactory::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (render_frame_host->GetParent())
    return;

  if (activation_level_ != ActivationLevel::DISABLED) {
    UMA_HISTOGRAM_COUNTS_1000(
        "SubresourceFilter.PageLoad.NumSubresourceLoads.Total",
        aggregated_document_statistics_.num_loads_total);
    UMA_HISTOGRAM_COUNTS_1000(
        "SubresourceFilter.PageLoad.NumSubresourceLoads.Evaluated",
        aggregated_document_statistics_.num_loads_evaluated);
    UMA_HISTOGRAM_COUNTS_1000(
        "SubresourceFilter.PageLoad.NumSubresourceLoads.MatchedRules",
        aggregated_document_statistics_.num_loads_matching_rules);
    UMA_HISTOGRAM_COUNTS_1000(
        "SubresourceFilter.PageLoad.NumSubresourceLoads.Disallowed",
        aggregated_document_statistics_.num_loads_disallowed);
  }

  if (measure_performance_) {
    DCHECK(activation_level_ != ActivationLevel::DISABLED);
    UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
        "SubresourceFilter.PageLoad.SubresourceEvaluation.TotalWallDuration",
        aggregated_document_statistics_.evaluation_total_wall_duration,
        base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(10),
        50);
    UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
        "SubresourceFilter.PageLoad.SubresourceEvaluation.TotalCPUDuration",
        aggregated_document_statistics_.evaluation_total_cpu_duration,
        base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(10),
        50);
  } else {
    DCHECK(aggregated_document_statistics_.evaluation_total_wall_duration
               .is_zero());
    DCHECK(aggregated_document_statistics_.evaluation_total_cpu_duration
               .is_zero());
  }
}

bool ContentSubresourceFilterDriverFactory::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ContentSubresourceFilterDriverFactory, message)
    IPC_MESSAGE_HANDLER(SubresourceFilterHostMsg_DidDisallowFirstSubresource,
                        OnFirstSubresourceLoadDisallowed)
    IPC_MESSAGE_HANDLER(SubresourceFilterHostMsg_DocumentLoadStatistics,
                        OnDocumentLoadStatistics)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ContentSubresourceFilterDriverFactory::ReadyToCommitNavigationInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    ui::PageTransition transition) {
  if (render_frame_host->GetParent()) {
    ActivateForFrameHostIfNeeded(render_frame_host, url);
    return;
  }

  RecordRedirectChainMatchPattern();

  if (ShouldWhitelistSiteOnReload() &&
      NavigationIsPageReload(url, referrer, transition)) {
    // Whitelist this host for the current as well as subsequent navigations.
    AddHostOfURLToWhitelistSet(url);
  }

  activation_decision_ = ComputeActivationDecisionForMainFrameURL(url);
  DCHECK(activation_decision_ != ActivationDecision::UNKNOWN);
  if (activation_decision_ != ActivationDecision::ACTIVATED) {
    ResetActivationState();
    return;
  }

  activation_level_ = GetMaximumActivationLevel();
  measure_performance_ = activation_level_ != ActivationLevel::DISABLED &&
                         ShouldMeasurePerformanceForPageLoad();
  ActivateForFrameHostIfNeeded(render_frame_host, url);
}

bool ContentSubresourceFilterDriverFactory::DidURLMatchCurrentActivationList(
    const GURL& url) const {
  auto match_types =
      activation_list_matches_.find(DistillURLToHostAndPath(url));
  return match_types != activation_list_matches_.end() &&
         match_types->second.find(GetCurrentActivationList()) !=
             match_types->second.end();
}

void ContentSubresourceFilterDriverFactory::AddActivationListMatch(
    const GURL& url,
    ActivationList match_type) {
  if (url.has_host() && url.SchemeIsHTTPOrHTTPS())
    activation_list_matches_[DistillURLToHostAndPath(url)].insert(match_type);
}

void ContentSubresourceFilterDriverFactory::RecordRedirectChainMatchPattern()
    const {
  int hits_pattern = 0;
  const int kInitialURLHitMask = 0x4;
  const int kRedirectURLHitMask = 0x2;
  const int kFinalURLHitMask = 0x1;
  if (navigation_chain_.size() > 1) {
    if (DidURLMatchCurrentActivationList(navigation_chain_.back()))
      hits_pattern |= kFinalURLHitMask;
    if (DidURLMatchCurrentActivationList(navigation_chain_.front()))
      hits_pattern |= kInitialURLHitMask;

    // Examine redirects.
    for (size_t i = 1; i < navigation_chain_.size() - 1; ++i) {
      if (DidURLMatchCurrentActivationList(navigation_chain_[i])) {
        hits_pattern |= kRedirectURLHitMask;
        break;
      }
    }
  } else {
    if (navigation_chain_.size() &&
        DidURLMatchCurrentActivationList(navigation_chain_.front())) {
      hits_pattern = 0x8;  // One url hit.
    }
  }
  if (!hits_pattern)
    return;
  UMA_HISTOGRAM_ENUMERATION(
      "SubresourceFilter.PageLoad.RedirectChainMatchPattern", hits_pattern,
      0x10 /* max value */);
  UMA_HISTOGRAM_COUNTS("SubresourceFilter.PageLoad.RedirectChainLength",
                       navigation_chain_.size());
}

}  // namespace subresource_filter
