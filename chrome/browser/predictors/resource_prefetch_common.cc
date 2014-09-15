// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_common.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace predictors {

const char kSpeculativePrefetchingTrialName[] =
    "SpeculativeResourcePrefetching";

bool IsSpeculativeResourcePrefetchingEnabled(
    Profile* profile,
    ResourcePrefetchPredictorConfig* config) {
  DCHECK(config);

  // Off the record - disabled.
  if (!profile || profile->IsOffTheRecord())
    return false;

  // If the user has explicitly disabled "predictive actions" - disabled.
  if (!profile->GetPrefs() ||
      !profile->GetPrefs()->GetBoolean(prefs::kNetworkPredictionEnabled)) {
    return false;
  }

  // The config has the default params already set. The command line with just
  // enable them with the default params.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSpeculativeResourcePrefetching)) {
    const std::string value =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
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

  std::string trial = base::FieldTrialList::FindFullName(
      kSpeculativePrefetchingTrialName);

  if (trial == "LearningHost") {
    config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
    return true;
  } else if (trial == "LearningURL") {
    config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
    return true;
  } else if (trial == "Learning") {
    config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
    return true;
  } else if (trial == "PrefetchingHost") {
    config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_PRFETCHING;
    return true;
  } else if (trial == "PrefetchingURL") {
    config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::URL_PREFETCHING;
    return true;
  } else if (trial == "Prefetching") {
    config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::URL_PREFETCHING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_PRFETCHING;
    return true;
  } else if (trial == "PrefetchingLowConfidence") {
    config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::URL_PREFETCHING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_PRFETCHING;

    config->min_url_visit_count = 1;
    config->min_resource_confidence_to_trigger_prefetch = 0.5f;
    config->min_resource_hits_to_trigger_prefetch = 1;
    return true;
  } else if (trial == "PrefetchingHighConfidence") {
    config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::URL_PREFETCHING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_PRFETCHING;

    config->min_url_visit_count = 3;
    config->min_resource_confidence_to_trigger_prefetch = 0.9f;
    config->min_resource_hits_to_trigger_prefetch = 3;
    return true;
  } else if (trial == "PrefetchingMoreResources") {
    config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::URL_PREFETCHING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_PRFETCHING;

    config->max_resources_per_entry = 100;
    return true;
  } else if (trial == "LearningSmallDB") {
    config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;

    config->max_urls_to_track = 200;
    config->max_hosts_to_track = 100;
    return true;
  } else if (trial == "PrefetchingSmallDB") {
    config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::URL_PREFETCHING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_PRFETCHING;

    config->max_urls_to_track = 200;
    config->max_hosts_to_track = 100;
    return true;
  } else if (trial == "PrefetchingSmallDBLowConfidence") {
    config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::URL_PREFETCHING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_PRFETCHING;

    config->max_urls_to_track = 200;
    config->max_hosts_to_track = 100;
    config->min_url_visit_count = 1;
    config->min_resource_confidence_to_trigger_prefetch = 0.5f;
    config->min_resource_hits_to_trigger_prefetch = 1;
    return true;
  } else if (trial == "PrefetchingSmallDBHighConfidence") {
    config->mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
    config->mode |= ResourcePrefetchPredictorConfig::URL_PREFETCHING;
    config->mode |= ResourcePrefetchPredictorConfig::HOST_PRFETCHING;

    config->max_urls_to_track = 200;
    config->max_hosts_to_track = 100;
    config->min_url_visit_count = 3;
    config->min_resource_confidence_to_trigger_prefetch = 0.9f;
    config->min_resource_hits_to_trigger_prefetch = 3;
    return true;
  }

  return false;
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
      render_frame_id(web_contents->GetRenderViewHost()->GetRoutingID()),
      main_frame_url(web_contents->GetURL()) {
}

bool NavigationID::is_valid() const {
  return render_process_id != -1 && render_frame_id != -1 &&
      !main_frame_url.is_empty();
}

bool NavigationID::operator<(const NavigationID& rhs) const {
  DCHECK(is_valid() && rhs.is_valid());
  if (render_process_id != rhs.render_process_id)
    return render_process_id < rhs.render_process_id;
  else if (render_frame_id != rhs.render_frame_id)
    return render_frame_id < rhs.render_frame_id;
  else
    return main_frame_url < rhs.main_frame_url;
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
      min_resource_confidence_to_trigger_prefetch(0.8f),
      min_resource_hits_to_trigger_prefetch(3),
      max_prefetches_inflight_per_navigation(24),
      max_prefetches_inflight_per_host_per_navigation(3) {
}

ResourcePrefetchPredictorConfig::~ResourcePrefetchPredictorConfig() {
}

bool ResourcePrefetchPredictorConfig::IsLearningEnabled() const {
  return IsURLLearningEnabled() || IsHostLearningEnabled();
}

bool ResourcePrefetchPredictorConfig::IsPrefetchingEnabled() const {
  return IsURLPrefetchingEnabled() || IsHostPrefetchingEnabled();
}

bool ResourcePrefetchPredictorConfig::IsURLLearningEnabled() const {
  return (mode & URL_LEARNING) > 0;
}

bool ResourcePrefetchPredictorConfig::IsHostLearningEnabled() const {
  return (mode & HOST_LEARNING) > 0;
}

bool ResourcePrefetchPredictorConfig::IsURLPrefetchingEnabled() const {
  return (mode & URL_PREFETCHING) > 0;
}

bool ResourcePrefetchPredictorConfig::IsHostPrefetchingEnabled() const {
  return (mode & HOST_PRFETCHING) > 0;
}

}  // namespace predictors
