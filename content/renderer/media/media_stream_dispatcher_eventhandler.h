// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_EVENTHANDLER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_EVENTHANDLER_H_

#include <string>

#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"

namespace content {

class CONTENT_EXPORT MediaStreamDispatcherEventHandler {
 public:
  // A new media stream have been created.
  virtual void OnStreamGenerated(int request_id,
                                 const std::string& label,
                                 const MediaStreamDevices& audio_devices,
                                 const MediaStreamDevices& video_devices) = 0;

  // Creation of a new media stream failed. The user might have denied access
  // to the requested devices or no device is available.
  virtual void OnStreamGenerationFailed(int request_id,
                                        MediaStreamRequestResult result) = 0;

  // A device has been stopped in the browser processes.
  virtual void OnDeviceStopped(const std::string& label,
                               const MediaStreamDevice& device) = 0;

 protected:
  virtual ~MediaStreamDispatcherEventHandler() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_EVENTHANDLER_H_
