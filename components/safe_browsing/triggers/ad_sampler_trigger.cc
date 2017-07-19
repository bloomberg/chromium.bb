// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/ad_sampler_trigger.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/safe_browsing/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(safe_browsing::AdSamplerTrigger);

namespace safe_browsing {

// Param name of the denominator for controlling sampling frequency.
const char kAdSamplerFrequencyDenominatorParam[] =
    "safe_browsing_ad_sampler_frequency_denominator";

// A frequency denominator with this value indicates sampling is disabled.
const size_t kSamplerFrequencyDisabled = 0;

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

bool DetectGoogleAd(content::NavigationHandle* navigation_handle) {
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
  content::RenderFrameHost* current_frame_host =
      navigation_handle->GetWebContents()->UnsafeFindFrameByFrameTreeNodeId(
          navigation_handle->GetFrameTreeNodeId());
  if (current_frame_host) {
    const std::string& frame_name = current_frame_host->GetFrameName();
    if (base::StartsWith(frame_name, "google_ads_iframe",
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(frame_name, "google_ads_frame",
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }

  const GURL& url = navigation_handle->GetURL();
  return url.host_piece() == "tpc.googlesyndication.com" &&
         base::StartsWith(url.path_piece(), "/safeframe",
                          base::CompareCase::SENSITIVE);
}

bool ShouldCheckForAd(const size_t frequency_denominator) {
  return frequency_denominator != kSamplerFrequencyDisabled &&
         (base::RandUint64() % frequency_denominator) == 0;
}

}  // namespace

AdSamplerTrigger::AdSamplerTrigger(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      sampler_frequency_denominator_(GetSamplerFrequencyDenominator()) {}

AdSamplerTrigger::~AdSamplerTrigger() {}

void AdSamplerTrigger::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(lpz): Add UMA metrics for how often we skip checking, find nothing, or
  // take a sample.
  if (ShouldCheckForAd(sampler_frequency_denominator_) &&
      DetectGoogleAd(navigation_handle)) {
    // TODO(lpz): Call into TriggerManager to sample the ad.
  }
}

}  // namespace safe_browsing
