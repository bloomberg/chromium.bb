// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_common.h"

#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace predictors {

NavigationID::NavigationID() : render_process_id(-1), render_view_id(-1) {
}

NavigationID::NavigationID(const NavigationID& other)
    : render_process_id(other.render_process_id),
      render_view_id(other.render_view_id),
      main_frame_url(other.main_frame_url),
      creation_time(other.creation_time) {
}

NavigationID::NavigationID(const content::WebContents& web_contents)
    : render_process_id(web_contents.GetRenderProcessHost()->GetID()),
      render_view_id(web_contents.GetRenderViewHost()->GetRoutingID()),
      main_frame_url(web_contents.GetURL()) {
}

bool NavigationID::is_valid() const {
  return render_process_id != -1 && render_view_id != -1 &&
      !main_frame_url.is_empty();
}

bool NavigationID::operator<(const NavigationID& rhs) const {
  DCHECK(is_valid() && rhs.is_valid());
  if (render_process_id != rhs.render_process_id)
    return render_process_id < rhs.render_process_id;
  else if (render_view_id != rhs.render_view_id)
    return render_view_id < rhs.render_view_id;
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
      render_view_id == other.render_view_id;
}

ResourcePrefetchPredictorConfig::ResourcePrefetchPredictorConfig()
    : max_navigation_lifetime_seconds(60),
      max_urls_to_track(500),
      max_hosts_to_track(200),
      min_url_visit_count(3),
      max_resources_per_entry(50),
      max_consecutive_misses(3),
      min_resource_confidence_to_trigger_prefetch(0.8f),
      min_resource_hits_to_trigger_prefetch(4),
      max_prefetches_inflight_per_navigation(24),
      max_prefetches_inflight_per_host_per_navigation(3) {
}

}  // namespace predictors
