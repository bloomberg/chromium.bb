// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation_request.h"

#include "chrome/browser/media/router/media_source_helper.h"

namespace media_router {

PresentationRequest::PresentationRequest(
    const RenderFrameHostId& render_frame_host_id,
    const std::string& presentation_url,
    const GURL& frame_url)
    : render_frame_host_id_(render_frame_host_id),
      presentation_url_(presentation_url),
      frame_url_(frame_url) {}

PresentationRequest::~PresentationRequest() = default;

bool PresentationRequest::Equals(const PresentationRequest& other) const {
  return render_frame_host_id_ == other.render_frame_host_id_ &&
         presentation_url_ == other.presentation_url_ &&
         frame_url_ == other.frame_url_;
}

MediaSource PresentationRequest::GetMediaSource() const {
  return MediaSourceForPresentationUrl(presentation_url_);
}

}  // namespace media_router
