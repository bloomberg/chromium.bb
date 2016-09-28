// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation_request.h"

#include "chrome/browser/media/router/media_source_helper.h"

namespace media_router {

PresentationRequest::PresentationRequest(
    const RenderFrameHostId& render_frame_host_id,
    const std::vector<std::string>& presentation_urls,
    const GURL& frame_url)
    : render_frame_host_id_(render_frame_host_id),
      presentation_urls_(presentation_urls),
      frame_url_(frame_url) {
  DCHECK(!presentation_urls_.empty());
}

PresentationRequest::PresentationRequest(const PresentationRequest& other) =
    default;

PresentationRequest::~PresentationRequest() = default;

bool PresentationRequest::Equals(const PresentationRequest& other) const {
  return render_frame_host_id_ == other.render_frame_host_id_ &&
         presentation_urls_ == other.presentation_urls_ &&
         frame_url_ == other.frame_url_;
}

std::vector<MediaSource> PresentationRequest::GetMediaSources() const {
  std::vector<MediaSource> sources;
  for (const auto& presentation_url : presentation_urls_)
    sources.push_back(MediaSourceForPresentationUrl(presentation_url));
  return sources;
}

}  // namespace media_router
