// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ad_metrics/frame_data.h"

#include <algorithm>
#include <limits>
#include <string>

#include "chrome/browser/page_load_metrics/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "url/gurl.h"

namespace {

// A frame with area less than kMinimumVisibleFrameArea is not considered
// visible.
const int kMinimumVisibleFrameArea = 25;

}  // namespace

// static
FrameData::ResourceMimeType FrameData::GetResourceMimeType(
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  if (blink::IsSupportedImageMimeType(resource->mime_type))
    return ResourceMimeType::kImage;
  if (blink::IsSupportedJavascriptMimeType(resource->mime_type))
    return ResourceMimeType::kJavascript;

  std::string top_level_type;
  std::string subtype;
  // Categorize invalid mime types as "Other".
  if (!net::ParseMimeTypeWithoutParameter(resource->mime_type, &top_level_type,
                                          &subtype)) {
    return ResourceMimeType::kOther;
  }
  if (top_level_type.compare("video") == 0)
    return ResourceMimeType::kVideo;
  if (top_level_type.compare("text") == 0 && subtype.compare("css") == 0)
    return ResourceMimeType::kCss;
  if (top_level_type.compare("text") == 0 && subtype.compare("html") == 0)
    return ResourceMimeType::kHtml;
  return ResourceMimeType::kOther;
}

FrameData::FrameData(FrameTreeNodeId frame_tree_node_id)
    : bytes_(0u),
      network_bytes_(0u),
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
  bytes_ += resource->delta_bytes;
  network_bytes_ += resource->delta_bytes;
  if (is_same_origin)
    same_origin_bytes_ += resource->delta_bytes;

  // Report cached resource body bytes to overall frame bytes.
  if (resource->is_complete && resource->was_fetched_via_cache) {
    bytes_ += resource->encoded_body_length;
    if (is_same_origin)
      same_origin_bytes_ += resource->encoded_body_length;
  }

  if (bytes_ > kFrameSizeInterventionByteThreshold &&
      user_activation_status_ == UserActivationStatus::kNoActivation) {
    size_intervention_status_ = FrameSizeInterventionStatus::kTriggered;
  }

  if (resource->reported_as_ad_resource) {
    ad_network_bytes_ += resource->delta_bytes;
    ad_bytes_ += resource->delta_bytes;
    // Report cached resource body bytes to overall frame bytes.
    if (resource->is_complete && resource->was_fetched_via_cache)
      ad_bytes_ += resource->encoded_body_length;

    ResourceMimeType mime_type = GetResourceMimeType(resource);
    ad_bytes_by_mime_[static_cast<size_t>(mime_type)] += resource->delta_bytes;
  }
}

void FrameData::AdjustAdBytes(int64_t unaccounted_ad_bytes,
                              ResourceMimeType mime_type) {
  ad_network_bytes_ += unaccounted_ad_bytes;
  ad_bytes_ += unaccounted_ad_bytes;
  ad_bytes_by_mime_[static_cast<size_t>(mime_type)] += unaccounted_ad_bytes;
}

void FrameData::SetFrameSize(gfx::Size frame_size) {
  frame_size_ = frame_size;
  UpdateFrameVisibility();
}

void FrameData::SetDisplayState(bool is_display_none) {
  is_display_none_ = is_display_none;
  UpdateFrameVisibility();
}

void FrameData::UpdateCpuUsage(base::TimeDelta update,
                               InteractiveStatus interactive) {
  cpu_by_interactive_period_[static_cast<size_t>(interactive)] += update;
  cpu_by_activation_period_[static_cast<size_t>(user_activation_status_)] +=
      update;
}

base::TimeDelta FrameData::GetInteractiveCpuUsage(
    InteractiveStatus status) const {
  return cpu_by_interactive_period_[static_cast<int>(status)];
}

base::TimeDelta FrameData::GetActivationCpuUsage(
    UserActivationStatus status) const {
  return cpu_by_activation_period_[static_cast<int>(status)];
}

base::TimeDelta FrameData::GetTotalCpuUsage() const {
  base::TimeDelta total_cpu_time;
  for (base::TimeDelta cpu_time : cpu_by_interactive_period_)
    total_cpu_time += cpu_time;
  return total_cpu_time;
}

void FrameData::SetReceivedUserActivation(base::TimeDelta foreground_duration) {
  user_activation_status_ = UserActivationStatus::kReceivedActivation;
  pre_activation_foreground_duration_ = foreground_duration;
}

size_t FrameData::GetAdNetworkBytesForMime(ResourceMimeType mime_type) const {
  return ad_bytes_by_mime_[static_cast<size_t>(mime_type)];
}

void FrameData::UpdateFrameVisibility() {
  visibility_ =
      !is_display_none_ &&
              frame_size_.GetCheckedArea().ValueOrDefault(
                  std::numeric_limits<int>::max()) >= kMinimumVisibleFrameArea
          ? FrameVisibility::kVisible
          : FrameVisibility::kNonVisible;
}
