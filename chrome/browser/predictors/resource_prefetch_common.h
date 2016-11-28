// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_COMMON_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_COMMON_H_

#include <stddef.h>

#include "base/time/time.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}

namespace predictors {

struct ResourcePrefetchPredictorConfig;

// Returns true if prefetching is enabled. And will initilize the |config|
// fields to the appropritate values.
bool IsSpeculativeResourcePrefetchingEnabled(
    Profile* profile,
    ResourcePrefetchPredictorConfig* config);

// Represents the type of key based on which prefetch data is stored.
enum PrefetchKeyType {
  PREFETCH_KEY_TYPE_HOST,
  PREFETCH_KEY_TYPE_URL
};

// Represents a single navigation for a render frame.
struct NavigationID {
  NavigationID();
  NavigationID(int render_process_id,
               int render_frame_id,
               const GURL& main_frame_url);
  NavigationID(const NavigationID& other);
  explicit NavigationID(content::WebContents* web_contents);
  bool operator<(const NavigationID& rhs) const;
  bool operator==(const NavigationID& rhs) const;

  bool IsSameRenderer(const NavigationID& other) const;

  // Returns true iff the render_process_id_, render_frame_id_ and
  // frame_url_ has been set correctly.
  bool is_valid() const;

  int render_process_id;
  int render_frame_id;
  GURL main_frame_url;

  // NOTE: Even though we store the creation time here, it is not used during
  // comparison of two NavigationIDs because it cannot always be determined
  // correctly.
  base::TimeTicks creation_time;
};

// Represents the config for the resource prefetch prediction algorithm. It is
// useful for running experiments.
struct ResourcePrefetchPredictorConfig {
  // Initializes the config with default values.
  ResourcePrefetchPredictorConfig();
  ResourcePrefetchPredictorConfig(const ResourcePrefetchPredictorConfig& other);
  ~ResourcePrefetchPredictorConfig();

  // The mode the prefetcher is running in. Forms a bit map.
  enum Mode {
    URL_LEARNING = 1 << 0,
    HOST_LEARNING = 1 << 1,
    URL_PREFETCHING = 1 << 2,  // Should also turn on URL_LEARNING.
    HOST_PREFETCHING = 1 << 3  // Should also turn on HOST_LEARNING.
  };
  int mode;

  // Helpers to deal with mode.
  bool IsLearningEnabled() const;
  bool IsPrefetchingEnabled(Profile* profile) const;
  bool IsURLLearningEnabled() const;
  bool IsHostLearningEnabled() const;
  bool IsURLPrefetchingEnabled(Profile* profile) const;
  bool IsHostPrefetchingEnabled(Profile* profile) const;

  bool IsLowConfidenceForTest() const;
  bool IsHighConfidenceForTest() const;
  bool IsMoreResourcesEnabledForTest() const;
  bool IsSmallDBEnabledForTest() const;

  // If a navigation hasn't seen a load complete event in this much time, it
  // is considered abandoned.
  size_t max_navigation_lifetime_seconds;

  // Size of LRU caches for the URL and host data.
  size_t max_urls_to_track;
  size_t max_hosts_to_track;

  // The number of times we should have seen a visit to this URL in history
  // to start tracking it. This is to ensure we don't bother with oneoff
  // entries. For hosts we track each one.
  size_t min_url_visit_count;

  // The maximum number of resources to store per entry.
  size_t max_resources_per_entry;
  // The number of consecutive misses after we stop tracking a resource URL.
  size_t max_consecutive_misses;

  // The minimum confidence (accuracy of hits) required for a resource to be
  // prefetched.
  float min_resource_confidence_to_trigger_prefetch;
  // The minimum number of times we must have a URL on record to prefetch it.
  size_t min_resource_hits_to_trigger_prefetch;

  // Maximum number of prefetches that can be inflight for a single navigation.
  size_t max_prefetches_inflight_per_navigation;
  // Maximum number of prefetches that can be inflight for a host for a single
  // navigation.
  size_t max_prefetches_inflight_per_host_per_navigation;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_COMMON_H_
