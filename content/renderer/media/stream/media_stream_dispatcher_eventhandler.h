// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_DISPATCHER_EVENTHANDLER_H_
#define CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_DISPATCHER_EVENTHANDLER_H_

#include <string>

#include "content/common/content_export.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace content {

class CONTENT_EXPORT MediaStreamDispatcherEventHandler {
 public:
  // A device has been stopped in the browser process.
  virtual void OnDeviceStopped(const blink::MediaStreamDevice& device) = 0;

  // Switch to the new device within the working session.
  virtual void OnDeviceChanged(const blink::MediaStreamDevice& old_device,
                               const blink::MediaStreamDevice& new_device) = 0;

 protected:
  virtual ~MediaStreamDispatcherEventHandler() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_DISPATCHER_EVENTHANDLER_H_
