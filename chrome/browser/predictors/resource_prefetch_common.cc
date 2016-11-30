// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_common.h"

#include <string>
#include <tuple>

#include "base/command_line.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace predictors {

namespace {

bool IsPrefetchingEnabledInternal(Profile* profile, int mode, int mask) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if ((mode & mask) == 0)
    return false;

  if (!profile || !profile->GetPrefs() ||
      chrome_browser_net::CanPrefetchAndPrerenderUI(profile->GetPrefs()) !=
          chrome_browser_net::NetworkPredictionStatus::ENABLED) {
    return false;
  }

  return true;
}

}  // namespace

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
      config->mode |= ResourcePrefetchPredictorConfig::LEARNING;
      return true;
    } else if (value ==
               switches::kSpeculativeResourcePrefetchingEnabledExternal) {
      config->mode |= ResourcePrefetchPredictorConfig::LEARNING |
                      ResourcePrefetchPredictorConfig::PREFETCHING_FOR_EXTERNAL;
      return true;
    } else if (value == switches::kSpeculativeResourcePrefetchingEnabled) {
      config->mode |=
          ResourcePrefetchPredictorConfig::LEARNING |
          ResourcePrefetchPredictorConfig::PREFETCHING_FOR_NAVIGATION |
          ResourcePrefetchPredictorConfig::PREFETCHING_FOR_EXTERNAL;
      return true;
    }
  }

  return false;
}

NavigationID::NavigationID()
    : render_process_id(-1),
      render_frame_id(-1) {
}

NavigationID::NavigationID(int render_process_id,
                           int render_frame_id,
                           const GURL& main_frame_url)
    : render_process_id(render_process_id),
      render_frame_id(render_frame_id),
      main_frame_url(main_frame_url) {}

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
  return (mode & LEARNING) > 0;
}

bool ResourcePrefetchPredictorConfig::IsPrefetchingEnabledForSomeOrigin(
    Profile* profile) const {
  int mask = PREFETCHING_FOR_NAVIGATION | PREFETCHING_FOR_EXTERNAL;
  return IsPrefetchingEnabledInternal(profile, mode, mask);
}

bool ResourcePrefetchPredictorConfig::IsPrefetchingEnabledForOrigin(
    Profile* profile,
    PrefetchOrigin origin) const {
  int mask = 0;
  switch (origin) {
    case PrefetchOrigin::NAVIGATION:
      mask = PREFETCHING_FOR_NAVIGATION;
      break;
    case PrefetchOrigin::EXTERNAL:
      mask = PREFETCHING_FOR_EXTERNAL;
      break;
  }
  return IsPrefetchingEnabledInternal(profile, mode, mask);
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
