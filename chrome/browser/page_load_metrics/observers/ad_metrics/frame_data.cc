// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ad_metrics/frame_data.h"
#include <algorithm>
#include "chrome/browser/page_load_metrics/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

FrameData::FrameData(FrameTreeNodeId frame_tree_node_id)
    : frame_bytes_(0u),
      frame_network_bytes_(0u),
      frame_tree_node_id_(frame_tree_node_id),
      origin_status_(OriginStatus::kUnknown),
      frame_navigated_(false),
      user_activation_status_(UserActivationStatus::kNoActivation),
      visibility_(FrameVisibility::kVisible),
      frame_size_(gfx::Size()) {}

FrameData::~FrameData() = default;

void FrameData::UpdateForNavigation(content::RenderFrameHost* render_frame_host,
                                    bool frame_navigated) {
  frame_navigated_ = frame_navigated;
  if (!render_frame_host)
    return;

  SetDisplayState(render_frame_host->IsFrameDisplayNone());
  if (render_frame_host->GetFrameSize())
    set_frame_size(*(render_frame_host->GetFrameSize()));

  // For frames triggered on render, their origin is their parent's origin.
  origin_status_ =
      AdsPageLoadMetricsObserver::IsSubframeSameOriginToMainFrame(
          render_frame_host, !frame_navigated /* use_parent_origin */)
          ? OriginStatus::kSame
          : OriginStatus::kCross;
}

void FrameData::ProcessResourceLoadInFrame(
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  frame_bytes_ += resource->delta_bytes;
  frame_network_bytes_ += resource->delta_bytes;

  // Report cached resource body bytes to overall frame bytes.
  if (resource->is_complete && resource->was_fetched_via_cache)
    frame_bytes_ += resource->encoded_body_length;
}

void FrameData::SetDisplayState(bool is_display_none) {
  if (is_display_none)
    visibility_ = FrameVisibility::kDisplayNone;
  else
    visibility_ = FrameVisibility::kVisible;
}
