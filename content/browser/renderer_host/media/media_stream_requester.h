// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_REQUESTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_REQUESTER_H_

#include <string>

#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"

namespace content {

// MediaStreamRequester must be implemented by the class requesting a new media
// stream to be opened. MediaStreamManager will use this interface to signal
// success and error for a request.
class CONTENT_EXPORT MediaStreamRequester {
 public:
  // Called as a reply of a successful call to GenerateStream.
  virtual void StreamGenerated(const std::string& label,
                               const StreamDeviceInfoArray& audio_devices,
                               const StreamDeviceInfoArray& video_devices) = 0;
  // Called if GenerateStream failed or if stream has been stopped by the user.
  // TODO(sergeyu): Rename this method and corresponding IPC message or maybe
  // add a separate IPC message.
  virtual void StreamGenerationFailed(const std::string& label) = 0;

  // Called as a reply of a successful call to EnumerateDevices.
  virtual void DevicesEnumerated(const std::string& label,
                                 const StreamDeviceInfoArray& devices) = 0;
  // Called as a reply of a successful call to OpenDevice.
  virtual void DeviceOpened(const std::string& label,
                            const StreamDeviceInfo& device_info) = 0;

 protected:
  virtual ~MediaStreamRequester() {
  }
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_REQUESTER_H_
