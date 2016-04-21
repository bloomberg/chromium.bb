// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_common.h"

#include <stdlib.h>
#include <tuple>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_split.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using base::FieldTrialList;
using std::string;
using std::vector;

namespace predictors {

const char kSpeculativePrefetchingTrialName[] =
    "SpeculativeResourcePrefetching";

/*
 * SpeculativeResourcePrefetching is a field trial, and its value must have the
 * following format: key1=value1:key2=value2:key3=value3
 * e.g. "Prefetching=Enabled:Predictor=Url:Confidence=High"
 * The function below extracts the value corresponding to a key provided from
 * the SpeculativeResourcePrefetching field trial.
 */
std::string GetFieldTrialSpecValue(string key) {
  std::string trial_name =
      FieldTrialList::FindFullName(kSpeculativePrefetchingTrialName);
  for (const base::StringPiece& element : base::SplitStringPiece(
           trial_name, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    std::vector<base::StringPiece> key_value = base::SplitStringPiece(
        element, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (key_value.size() == 2 && key_value[0] == key)
      return key_value[1].as_string();
  }
  return string();
}

bool IsSpeculativeResourcePrefetchingEnabled(
    Profile* profile,
    ResourcePrefetchPredictorConfig* config) {
  DCHECK(config);

  // Off the record - disabled.
  if (!profile || profile->IsOffTheRecord())
    return false;

  // Enabled by command line switch. The config has the default params already
  // set. The command line with just enable them with the default params.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSpeculativeResourcePrefetching)) {
    const std::string value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kSpeculativeResourcePrefetching);

    if (value == switches::kSpeculativeResourcePrefetchingDisabled) {
      return false;
    } else if (value == switches::kSpeculativeResourcePrefetchingLearning) {
      config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
      config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
      return true;
    } else if (value == switches::kSpeculativeResourcePrefetchingEnabled) {
      config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
      config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
      config->mode |= ResourcePrefetchPredictorConfig::URL_PREFETCHING;
      config->mode |= ResourcePrefetchPredictorConfig::HOST_PRFETCHING;
      return true;
    }
  }

  // Disable if no field trial is specified.
  std::string trial = base::FieldTrialList::FindFullName(
        kSpeculativePrefetchingTrialName);
  if (trial.empty())
    return false;

  // Enabled by field trial.
  std::string spec_prefetching = GetFieldTrialSpecValue("Prefetching");
  std::string spec_predictor = GetFieldTrialSpecValue("Predictor");
  std::string spec_confidence = GetFieldTrialSpecValue("Confidence");
  std::string spec_more_resources = GetFieldTrialSpecValue("MoreResources");
  std::string spec_small_db = GetFieldTrialSpecValue("SmallDB");

  if (spec_prefetching == "Learning") {
    if (spec_predictor == "Url") {
      config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
    } else if (spec_predictor == "Host") {
      config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
    } else {
      // Default: both Url and Host
      config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
      config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
    }
  } else if (spec_prefetching == "Enabled") {
    if (spec_predictor == "Url") {
      config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
      config->mode |= ResourcePrefetchPredictorConfig::URL_PREFETCHING;
    } else if (spec_predictor == "Host") {
      config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
      config->mode |= ResourcePrefetchPredictorConfig::HOST_PRFETCHING;
    } else {
      // Default: both Url and Host
      config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
      config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
      config->mode |= ResourcePrefetchPredictorConfig::URL_PREFETCHING;
      config->mode |= ResourcePrefetchPredictorConfig::HOST_PRFETCHING;
    }
  } else {
    // Default: spec_prefetching == "Disabled"
    return false;
  }

  if (spec_confidence == "Low") {
    config->min_url_visit_count = 1;
    config->min_resource_confidence_to_trigger_prefetch = 0.5f;
    config->min_resource_hits_to_trigger_prefetch = 1;
  } else if (spec_confidence == "High") {
    config->min_url_visit_count = 3;
    config->min_resource_confidence_to_trigger_prefetch = 0.9f;
    config->min_resource_hits_to_trigger_prefetch = 3;
  } else {
    // default
    config->min_url_visit_count = 2;
    config->min_resource_confidence_to_trigger_prefetch = 0.7f;
    config->min_resource_hits_to_trigger_prefetch = 2;
  }

  if (spec_more_resources == "Enabled") {
    config->max_resources_per_entry = 100;
  }

  if (spec_small_db == "Enabled") {
    config->max_urls_to_track = 200;
    config->max_hosts_to_track = 100;
  }

  return true;
}

NavigationID::NavigationID()
    : render_process_id(-1),
      render_frame_id(-1) {
}

NavigationID::NavigationID(const NavigationID& other)
    : render_process_id(other.render_process_id),
      render_frame_id(other.render_frame_id),
      main_frame_url(other.main_frame_url),
      creation_time(other.creation_time) {
}

NavigationID::NavigationID(content::WebContents* web_contents)
    : render_process_id(web_contents->GetRenderProcessHost()->GetID()),
      render_frame_id(web_contents->GetMainFrame()->GetRoutingID()),
      main_frame_url(web_contents->GetURL()) {
}

bool NavigationID::is_valid() const {
  return render_process_id != -1 && render_frame_id != -1 &&
      !main_frame_url.is_empty();
}

bool NavigationID::operator<(const NavigationID& rhs) const {
  DCHECK(is_valid() && rhs.is_valid());
  return std::tie(render_process_id, render_frame_id, main_frame_url) <
    std::tie(rhs.render_process_id, rhs.render_frame_id, rhs.main_frame_url);
}

bool NavigationID::operator==(const NavigationID& rhs) const {
  DCHECK(is_valid() && rhs.is_valid());
  return IsSameRenderer(rhs) && main_frame_url == rhs.main_frame_url;
}

bool NavigationID::IsSameRenderer(const NavigationID& other) const {
  DCHECK(is_valid() && other.is_valid());
  return render_process_id == other.render_process_id &&
      render_frame_id == other.render_frame_id;
}

ResourcePrefetchPredictorConfig::ResourcePrefetchPredictorConfig()
    : mode(0),
      max_navigation_lifetime_seconds(60),
      max_urls_to_track(500),
      max_hosts_to_track(200),
      min_url_visit_count(2),
      max_resources_per_entry(50),
      max_consecutive_misses(3),
      min_resource_confidence_to_trigger_prefetch(0.7f),
      min_resource_hits_to_trigger_prefetch(2),
      max_prefetches_inflight_per_navigation(24),
      max_prefetches_inflight_per_host_per_navigation(3) {
}

ResourcePrefetchPredictorConfig::ResourcePrefetchPredictorConfig(
    const ResourcePrefetchPredictorConfig& other) = default;

ResourcePrefetchPredictorConfig::~ResourcePrefetchPredictorConfig() {
}

bool ResourcePrefetchPredictorConfig::IsLearningEnabled() const {
  return IsURLLearningEnabled() || IsHostLearningEnabled();
}

bool ResourcePrefetchPredictorConfig::IsPrefetchingEnabled(
    Profile* profile) const {
  return IsURLPrefetchingEnabled(profile) || IsHostPrefetchingEnabled(profile);
}

bool ResourcePrefetchPredictorConfig::IsURLLearningEnabled() const {
  return (mode & URL_LEARNING) > 0;
}

bool ResourcePrefetchPredictorConfig::IsHostLearningEnabled() const {
  return (mode & HOST_LEARNING) > 0;
}

bool ResourcePrefetchPredictorConfig::IsURLPrefetchingEnabled(
    Profile* profile) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!profile || !profile->GetPrefs() ||
      chrome_browser_net::CanPrefetchAndPrerenderUI(profile->GetPrefs()) !=
      chrome_browser_net::NetworkPredictionStatus::ENABLED) {
    return false;
  }
  return (mode & URL_PREFETCHING) > 0;
}

bool ResourcePrefetchPredictorConfig::IsHostPrefetchingEnabled(
    Profile* profile) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!profile || !profile->GetPrefs() ||
      chrome_browser_net::CanPrefetchAndPrerenderUI(profile->GetPrefs()) !=
      chrome_browser_net::NetworkPredictionStatus::ENABLED) {
    return false;
  }
  return (mode & HOST_PRFETCHING) > 0;
}

bool ResourcePrefetchPredictorConfig::IsLowConfidenceForTest() const {
  return min_url_visit_count == 1 &&
      std::abs(min_resource_confidence_to_trigger_prefetch - 0.5f) < 1e-6 &&
      min_resource_hits_to_trigger_prefetch == 1;
}

bool ResourcePrefetchPredictorConfig::IsHighConfidenceForTest() const {
  return min_url_visit_count == 3 &&
      std::abs(min_resource_confidence_to_trigger_prefetch - 0.9f) < 1e-6 &&
      min_resource_hits_to_trigger_prefetch == 3;
}

bool ResourcePrefetchPredictorConfig::IsMoreResourcesEnabledForTest() const {
  return max_resources_per_entry == 100;
}

bool ResourcePrefetchPredictorConfig::IsSmallDBEnabledForTest() const {
  return max_urls_to_track == 200 && max_hosts_to_track == 100;
}

}  // namespace predictors
