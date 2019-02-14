// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ad_metrics/frame_data.h"
#include <algorithm>
#include "chrome/browser/page_load_metrics/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

// A frame with area less than kMinimumVisibleFrameArea is not considered
// visible.
const int kMinimumVisibleFrameArea = 25;

}  // namespace

FrameData::FrameData(FrameTreeNodeId frame_tree_node_id)
    : frame_bytes_(0u),
      frame_network_bytes_(0u),
      same_origin_bytes_(0u),
      frame_tree_node_id_(frame_tree_node_id),
      origin_status_(OriginStatus::kUnknown),
      frame_navigated_(false),
      user_activation_status_(UserActivationStatus::kNoActivation),
      is_display_none_(false),
      visibility_(FrameVisibility::kVisible),
      frame_size_(gfx::Size()),
      size_intervention_status_(FrameSizeInterventionStatus::kNone) {}

FrameData::~FrameData() = default;

void FrameData::UpdateForNavigation(content::RenderFrameHost* render_frame_host,
                                    bool frame_navigated) {
  frame_navigated_ = frame_navigated;
  if (!render_frame_host)
    return;

  SetDisplayState(render_frame_host->IsFrameDisplayNone());
  if (render_frame_host->GetFrameSize())
    SetFrameSize(*(render_frame_host->GetFrameSize()));

  // For frames triggered on render, their origin is their parent's origin.
  origin_status_ =
      AdsPageLoadMetricsObserver::IsSubframeSameOriginToMainFrame(
          render_frame_host, !frame_navigated /* use_parent_origin */)
          ? OriginStatus::kSame
          : OriginStatus::kCross;

  origin_ = frame_navigated
                ? render_frame_host->GetLastCommittedOrigin()
                : render_frame_host->GetParent()->GetLastCommittedOrigin();
}

void FrameData::ProcessResourceLoadInFrame(
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  bool is_same_origin = origin_.IsSameOriginWith(resource->origin);
  frame_bytes_ += resource->delta_bytes;
  frame_network_bytes_ += resource->delta_bytes;
  if (is_same_origin)
    same_origin_bytes_ += resource->delta_bytes;

  // Report cached resource body bytes to overall frame bytes.
  if (resource->is_complete && resource->was_fetched_via_cache) {
    frame_bytes_ += resource->encoded_body_length;
    if (is_same_origin)
      same_origin_bytes_ += resource->encoded_body_length;
  }

  if (frame_bytes_ > kFrameSizeInterventionByteThreshold &&
      user_activation_status_ == UserActivationStatus::kNoActivation) {
    size_intervention_status_ = FrameSizeInterventionStatus::kTriggered;
  }
}

void FrameData::SetFrameSize(gfx::Size frame_size) {
  frame_size_ = frame_size;
  UpdateFrameVisibility();
}

void FrameData::SetDisplayState(bool is_display_none) {
  is_display_none_ = is_display_none;
  UpdateFrameVisibility();
}

void FrameData::UpdateFrameVisibility() {
  visibility_ =
      !is_display_none_ && frame_size_.GetArea() >= kMinimumVisibleFrameArea
          ? FrameVisibility::kVisible
          : FrameVisibility::kNonVisible;
}
