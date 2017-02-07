// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_REQUEST_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_REQUEST_H_

#include <string>
#include <vector>

#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/render_frame_host_id.h"
#include "url/origin.h"

namespace media_router {

// Represents a presentation request made from a render frame. Contains the
// presentation URL of the request, and information on the originating frame.
class PresentationRequest {
 public:
  PresentationRequest(const RenderFrameHostId& render_frame_host_id,
                      const std::vector<GURL>& presentation_urls,
                      const url::Origin& frame_origin);
  PresentationRequest(const PresentationRequest& other);
  ~PresentationRequest();

  bool Equals(const PresentationRequest& other) const;

  // Helper method to get the MediaSources for |presentation_urls_|.
  std::vector<MediaSource> GetMediaSources() const;

  const RenderFrameHostId& render_frame_host_id() const {
    return render_frame_host_id_;
  }
  const std::vector<GURL>& presentation_urls() const {
    return presentation_urls_;
  }
  const url::Origin& frame_origin() const { return frame_origin_; }

 private:
  // ID of RenderFrameHost that initiated the request.
  const RenderFrameHostId render_frame_host_id_;

  // URLs of presentation.
  const std::vector<GURL> presentation_urls_;

  // Origin of frame from which the request was initiated.
  const url::Origin frame_origin_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_REQUEST_H_
