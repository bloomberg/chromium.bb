// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_REQUESTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_REQUESTER_H_

#include <string>

#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"

namespace content {

// MediaStreamRequester must be implemented by the class requesting a new media
// stream to be opened. MediaStreamManager will use this interface to signal
// success and error for a request.
class CONTENT_EXPORT MediaStreamRequester {
 public:
  // Called as a reply of a successful call to GenerateStream.
  virtual void StreamGenerated(int render_frame_id,
                               int page_request_id,
                               const std::string& label,
                               const MediaStreamDevices& audio_devices,
                               const MediaStreamDevices& video_devices) = 0;
  // Called if GenerateStream failed.
  virtual void StreamGenerationFailed(int render_frame_id,
                                      int page_request_id,
                                      MediaStreamRequestResult result) = 0;
  // Called if a device has been stopped by a user from UI or the device
  // has become unavailable.  |render_frame_id| is the render frame that
  // requested the device and |label| is the label of the request.
  virtual void DeviceStopped(int render_frame_id,
                             const std::string& label,
                             const MediaStreamDevice& device) = 0;

 protected:
  virtual ~MediaStreamRequester() {
  }
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_REQUESTER_H_
