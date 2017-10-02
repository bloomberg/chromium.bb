// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/ad_sampler_trigger.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/triggers/trigger_manager.h"
#include "components/safe_browsing/triggers/trigger_throttler.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(safe_browsing::AdSamplerTrigger);

namespace safe_browsing {

// Param name of the denominator for controlling sampling frequency.
const char kAdSamplerFrequencyDenominatorParam[] =
    "safe_browsing_ad_sampler_frequency_denominator";

// A frequency denominator with this value indicates sampling is disabled.
const size_t kSamplerFrequencyDisabled = 0;

// Number of milliseconds to allow data collection to run before sending a
// report (since this trigger runs in the background).
const int64_t kAdSampleCollectionPeriodMilliseconds = 5000;

// Metric for tracking what the Ad Sampler trigger does on each navigation.
const char kAdSamplerTriggerActionMetricName[] =
    "SafeBrowsing.Triggers.AdSampler.Action";

namespace {

size_t GetSamplerFrequencyDenominator() {
  if (!base::FeatureList::IsEnabled(kAdSamplerTriggerFeature))
    return kSamplerFrequencyDisabled;

  const std::string sampler_frequency_denominator =
      base::GetFieldTrialParamValueByFeature(
          kAdSamplerTriggerFeature, kAdSamplerFrequencyDenominatorParam);
  int result;
  return base::StringToInt(sampler_frequency_denominator, &result)
             ? result
             : kSamplerFrequencyDisabled;
}

bool DetectGoogleAd(content::RenderFrameHost* render_frame_host,
                    const GURL& frame_url) {
  // TODO(crbug.com/742397): This function is temporarily copied from
  // c/b/page_load_metrics/observers/ads_page_load_metrics_observer.cc
  // This code should be updated to use shared infrastructure when available.

  // In case the navigation aborted, look up the RFH by the Frame Tree Node
  // ID. It returns the committed frame host or the initial frame host for the
  // frame if no committed host exists. Using a previous host is fine because
  // once a frame has an ad we always consider it to have an ad.
  // We use the unsafe method of FindFrameByFrameTreeNodeId because we're not
  // concerned with which process the frame lives on (we only want to know if an
  // ad could be present on the page right now).
  if (render_frame_host) {
    const std::string& frame_name = render_frame_host->GetFrameName();
    if (base::StartsWith(frame_name, "google_ads_iframe",
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(frame_name, "google_ads_frame",
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }

  return frame_url.host_piece() == "tpc.googlesyndication.com" &&
         base::StartsWith(frame_url.path_piece(), "/safeframe",
                          base::CompareCase::SENSITIVE);
}

bool ShouldSampleAd(const size_t frequency_denominator) {
  return frequency_denominator != kSamplerFrequencyDisabled &&
         (base::RandUint64() % frequency_denominator) == 0;
}

}  // namespace

AdSamplerTrigger::AdSamplerTrigger(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager,
    PrefService* prefs,
    net::URLRequestContextGetter* request_context,
    history::HistoryService* history_service)
    : content::WebContentsObserver(web_contents),
      sampler_frequency_denominator_(GetSamplerFrequencyDenominator()),
      finish_report_delay_ms_(kAdSampleCollectionPeriodMilliseconds),
      trigger_manager_(trigger_manager),
      prefs_(prefs),
      request_context_(request_context),
      history_service_(history_service) {}

AdSamplerTrigger::~AdSamplerTrigger() {}

// static
void AdSamplerTrigger::CreateForWebContents(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager,
    PrefService* prefs,
    net::URLRequestContextGetter* request_context,
    history::HistoryService* history_service) {
  DCHECK(web_contents);
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(UserDataKey(),
                              base::WrapUnique(new AdSamplerTrigger(
                                  web_contents, trigger_manager, prefs,
                                  request_context, history_service)));
  }
}

void AdSamplerTrigger::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  UMA_HISTOGRAM_ENUMERATION(kAdSamplerTriggerActionMetricName, TRIGGER_CHECK,
                            MAX_ACTIONS);
  // We are using light-weight ad detection logic here so it's safe to do the
  // check on each navigation for the sake of metrics.
  if (!DetectGoogleAd(render_frame_host, validated_url)) {
    UMA_HISTOGRAM_ENUMERATION(kAdSamplerTriggerActionMetricName,
                              NO_SAMPLE_NO_AD, MAX_ACTIONS);
    return;
  }
  if (!ShouldSampleAd(sampler_frequency_denominator_)) {
    UMA_HISTOGRAM_ENUMERATION(kAdSamplerTriggerActionMetricName,
                              NO_SAMPLE_AD_SKIPPED_FOR_FREQUENCY, MAX_ACTIONS);
    return;
  }

  SBErrorOptions error_options =
      TriggerManager::GetSBErrorDisplayOptions(*prefs_, *web_contents());

  security_interstitials::UnsafeResource resource;
  resource.threat_type = SB_THREAT_TYPE_AD_SAMPLE;
  resource.url = web_contents()->GetURL();
  resource.web_contents_getter = resource.GetWebContentsGetter(
      web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID());

  if (!trigger_manager_->StartCollectingThreatDetails(
          TriggerType::AD_SAMPLE, web_contents(), resource, request_context_,
          history_service_, error_options)) {
    UMA_HISTOGRAM_ENUMERATION(kAdSamplerTriggerActionMetricName,
                              NO_SAMPLE_COULD_NOT_START_REPORT, MAX_ACTIONS);
    return;
  }

  // Call into TriggerManager to finish the reports after a short delay. Any
  // ads that are detected during this delay will be rejected by TriggerManager
  // because a report is already being collected, so we won't send multiple
  // reports for the same page.
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          IgnoreResult(&TriggerManager::FinishCollectingThreatDetails),
          base::Unretained(trigger_manager_), TriggerType::AD_SAMPLE,
          base::Unretained(web_contents()), base::TimeDelta(),
          /*did_proceed=*/false, /*num_visits=*/0, error_options),
      base::TimeDelta::FromMilliseconds(finish_report_delay_ms_));

  UMA_HISTOGRAM_ENUMERATION(kAdSamplerTriggerActionMetricName, AD_SAMPLED,
                            MAX_ACTIONS);
}

}  // namespace safe_browsing
