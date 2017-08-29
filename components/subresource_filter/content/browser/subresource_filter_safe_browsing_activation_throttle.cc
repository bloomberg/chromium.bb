// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"

#include <sstream>
#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/subresource_filter/content/browser/content_activation_list_utils.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace subresource_filter {

SubresourceFilterSafeBrowsingActivationThrottle::
    SubresourceFilterSafeBrowsingActivationThrottle(
        content::NavigationHandle* handle,
        SubresourceFilterClient* client,
        scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
        scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
            database_manager)
    : NavigationThrottle(handle),
      io_task_runner_(std::move(io_task_runner)),
      // The throttle can be created without a valid database manager. If so, it
      // becomes a pass-through throttle and should never defer.
      database_client_(database_manager
                           ? new SubresourceFilterSafeBrowsingClient(
                                 std::move(database_manager),
                                 AsWeakPtr(),
                                 io_task_runner_,
                                 base::ThreadTaskRunnerHandle::Get())
                           : nullptr,
                       base::OnTaskRunnerDeleter(io_task_runner_)),
      client_(client) {
  DCHECK(handle->IsInMainFrame());

  CheckCurrentUrl();
  // Check added to investigate crbug.com/733099.
  CHECK(!database_client_ || !check_results_.empty());
}

SubresourceFilterSafeBrowsingActivationThrottle::
    ~SubresourceFilterSafeBrowsingActivationThrottle() {
  // The last check could be ongoing when the navigation is cancelled.
  if (check_results_.empty() || !check_results_.back().finished)
    return;
  ActivationList matched_list = GetListForThreatTypeAndMetadata(
      check_results_.back().threat_type, check_results_.back().threat_metadata);

  // TODO(csharrison): Log more metrics based on check_results_.
  UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.PageLoad.ActivationList",
                            matched_list,
                            static_cast<int>(ActivationList::LAST) + 1);

  size_t chain_size = check_results_.size();
  switch (matched_list) {
    case ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL:
      UMA_HISTOGRAM_COUNTS(
          "SubresourceFilter.PageLoad.RedirectChainLength."
          "SocialEngineeringAdsInterstitial",
          chain_size);
      break;
    case ActivationList::PHISHING_INTERSTITIAL:
      UMA_HISTOGRAM_COUNTS(
          "SubresourceFilter.PageLoad.RedirectChainLength."
          "PhishingInterstitial",
          chain_size);
      break;
    case ActivationList::SUBRESOURCE_FILTER:
      UMA_HISTOGRAM_COUNTS(
          "SubresourceFilter.PageLoad.RedirectChainLength."
          "SubresourceFilterOnly",
          chain_size);
      break;
    case ActivationList::BETTER_ADS:
      UMA_HISTOGRAM_COUNTS(
          "SubresourceFilter.PageLoad.RedirectChainLength."
          "BetterAds",
          chain_size);
      break;
    case ActivationList::ABUSIVE_ADS:
      UMA_HISTOGRAM_COUNTS(
          "SubresourceFilter.PageLoad.RedirectChainLength."
          "AbusiveAds",
          chain_size);
      break;
    case ActivationList::ALL_ADS:
      UMA_HISTOGRAM_COUNTS(
          "SubresourceFilter.PageLoad.RedirectChainLength."
          "AllAds",
          chain_size);
      break;
    default:
      break;
  }
}

bool SubresourceFilterSafeBrowsingActivationThrottle::NavigationIsPageReload(
    content::NavigationHandle* handle) {
  return ui::PageTransitionCoreTypeIs(handle->GetPageTransition(),
                                      ui::PAGE_TRANSITION_RELOAD) ||
         // Some pages 'reload' from JavaScript by navigating to themselves.
         handle->GetURL() == handle->GetReferrer().url;
}

content::NavigationThrottle::ThrottleCheckResult
SubresourceFilterSafeBrowsingActivationThrottle::WillStartRequest() {
  will_start_request_called_ = true;
  return content::NavigationThrottle::ThrottleCheckResult::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
SubresourceFilterSafeBrowsingActivationThrottle::WillRedirectRequest() {
  CheckCurrentUrl();
  // Check added to investigate crbug.com/733099.
  CHECK(!database_client_ || !check_results_.empty());
  return content::NavigationThrottle::ThrottleCheckResult::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
SubresourceFilterSafeBrowsingActivationThrottle::WillProcessResponse() {
  // Checks added to investigate crbug.com/733099.
  CHECK(will_start_request_called_);
  CHECK(!database_client_ || !check_results_.empty());

  // No need to defer the navigation if the check already happened.
  if (!database_client_ || check_results_.back().finished) {
    NotifyResult();
    return content::NavigationThrottle::ThrottleCheckResult::PROCEED;
  }
  CHECK(!deferring_);
  deferring_ = true;
  defer_time_ = base::TimeTicks::Now();
  return content::NavigationThrottle::ThrottleCheckResult::DEFER;
}

const char*
SubresourceFilterSafeBrowsingActivationThrottle::GetNameForLogging() {
  return "SubresourceFilterSafeBrowsingActivationThrottle";
}

void SubresourceFilterSafeBrowsingActivationThrottle::OnCheckUrlResultOnUI(
    const SubresourceFilterSafeBrowsingClient::CheckResult& result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  size_t request_id = result.request_id;
  DCHECK_LT(request_id, check_results_.size());

  auto& stored_result = check_results_.at(request_id);
  CHECK(!stored_result.finished);
  stored_result = result;
  if (deferring_ && request_id == check_results_.size() - 1) {
    NotifyResult();

    deferring_ = false;
    Resume();
  }
}

void SubresourceFilterSafeBrowsingActivationThrottle::CheckCurrentUrl() {
  if (!database_client_)
    return;
  check_results_.emplace_back();
  size_t id = check_results_.size() - 1;
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SubresourceFilterSafeBrowsingClient::CheckUrlOnIO,
                            base::Unretained(database_client_.get()),
                            navigation_handle()->GetURL(), id));
}

void SubresourceFilterSafeBrowsingActivationThrottle::NotifyResult() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SubresourceFilterSafeBrowsingActivationThrottle::NotifyResult");
  auto* driver_factory = ContentSubresourceFilterDriverFactory::FromWebContents(
      navigation_handle()->GetWebContents());
  DCHECK(driver_factory);
  if (driver_factory->GetActivationOptionsForLastCommittedPageLoad()
          .should_whitelist_site_on_reload &&
      NavigationIsPageReload(navigation_handle())) {
    // Whitelist this host for the current as well as subsequent navigations.
    client_->WhitelistInCurrentWebContents(navigation_handle()->GetURL());
  }

  Configuration matched_configuration;
  ActivationDecision activation_decision =
      ComputeActivation(&matched_configuration);
  Configuration::ActivationOptions& matched_options =
      matched_configuration.activation_options;

  // For forced activation, keep all the options except activation_level.
  if (client_->ForceActivationInCurrentWebContents()) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"), "ActivationForced");
    activation_decision = ActivationDecision::ACTIVATED;
    matched_options.activation_level = ActivationLevel::ENABLED;
  }

  // Check for whitelisted status last, so that the client gets an accurate
  // indication of whether there would be activation otherwise.
  bool whitelisted = client_->OnPageActivationComputed(
      navigation_handle(),
      matched_options.activation_level == ActivationLevel::ENABLED);

  // Only reset the activation decision reason if we would have activated.
  if (whitelisted && activation_decision == ActivationDecision::ACTIVATED) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"), "ActivationWhitelisted");
    activation_decision = ActivationDecision::URL_WHITELISTED;
    matched_options = Configuration::ActivationOptions();
  }

  driver_factory->NotifyPageActivationComputed(
      navigation_handle(), activation_decision, matched_configuration);

  base::TimeDelta delay = defer_time_.is_null()
                              ? base::TimeDelta::FromMilliseconds(0)
                              : base::TimeTicks::Now() - defer_time_;
  UMA_HISTOGRAM_TIMES("SubresourceFilter.PageLoad.SafeBrowsingDelay", delay);

  // Log a histogram for the delay we would have introduced if the throttle only
  // speculatively checks URLs on WillStartRequest. This is only different from
  // the actual delay if there was at least one redirect.
  base::TimeDelta no_redirect_speculation_delay =
      check_results_.size() > 1 ? check_results_.back().check_time : delay;
  UMA_HISTOGRAM_TIMES(
      "SubresourceFilter.PageLoad.SafeBrowsingDelay.NoRedirectSpeculation",
      no_redirect_speculation_delay);
}

ActivationDecision
SubresourceFilterSafeBrowsingActivationThrottle::ComputeActivation(
    Configuration* configuration) {
  const GURL& url(navigation_handle()->GetURL());
  ActivationList matched_list = ActivationList::NONE;
  DCHECK(!database_client_ || !check_results_.empty());
  if (!check_results_.empty()) {
    DCHECK(check_results_.back().finished);
    matched_list =
        GetListForThreatTypeAndMetadata(check_results_.back().threat_type,
                                        check_results_.back().threat_metadata);
  }

  const auto config_list = GetEnabledConfigurations();
  bool scheme_is_http_or_https = url.SchemeIsHTTPOrHTTPS();
  const auto highest_priority_activated_config =
      std::find_if(config_list->configs_by_decreasing_priority().begin(),
                   config_list->configs_by_decreasing_priority().end(),
                   [&url, scheme_is_http_or_https, matched_list,
                    this](const Configuration& config) {
                     return DoesMainFrameURLSatisfyActivationConditions(
                         url, scheme_is_http_or_https,
                         config.activation_conditions, matched_list);
                   });

  bool has_activated_config =
      highest_priority_activated_config !=
      config_list->configs_by_decreasing_priority().end();
  TRACE_EVENT1(
      TRACE_DISABLED_BY_DEFAULT("loading"),
      "SubresourceFilterSafeBrowsingActivationThrottle::ComputeActivation",
      "highest_priority_activated_config",
      has_activated_config
          ? highest_priority_activated_config->ToTracedValue()
          : base::MakeUnique<base::trace_event::TracedValue>());

  if (!has_activated_config)
    return ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET;

  const Configuration::ActivationOptions& activation_options =
      highest_priority_activated_config->activation_options;
  if (!scheme_is_http_or_https &&
      activation_options.activation_level != ActivationLevel::DISABLED) {
    return ActivationDecision::UNSUPPORTED_SCHEME;
  }

  *configuration = *highest_priority_activated_config;
  return activation_options.activation_level == ActivationLevel::DISABLED
             ? ActivationDecision::ACTIVATION_DISABLED
             : ActivationDecision::ACTIVATED;
}

bool SubresourceFilterSafeBrowsingActivationThrottle::
    DoesMainFrameURLSatisfyActivationConditions(
        const GURL& url,
        bool scheme_is_http_or_https,
        const Configuration::ActivationConditions& conditions,
        ActivationList matched_list) const {
  // Avoid copies when tracing disabled.
  auto list_to_string = [](ActivationList activation_list) {
    std::ostringstream matched_list_stream;
    matched_list_stream << activation_list;
    return matched_list_stream.str();
  };
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SubresourceFilterSafeBrowsingActivationThrottle::"
               "DoesMainFrameURLSatisfyActivationConditions",
               "matched_list", list_to_string(matched_list), "conditions",
               conditions.ToTracedValue());
  switch (conditions.activation_scope) {
    case ActivationScope::ALL_SITES:
      return true;
    case ActivationScope::ACTIVATION_LIST:
      // ACTIVATION_LIST does not support non http/s URLs.
      if (!scheme_is_http_or_https)
        return false;
      if (matched_list == ActivationList::NONE)
        return false;
      if (conditions.activation_list == matched_list)
        return true;
      if (conditions.activation_list == ActivationList::PHISHING_INTERSTITIAL &&
          matched_list == ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL) {
        // Handling special case, where activation on the phishing sites also
        // mean the activation on the sites with social engineering metadata.
        return true;
      }
      return false;
    case ActivationScope::NO_SITES:
      return false;
  }
  NOTREACHED();
  return false;
}

}  //  namespace subresource_filter
