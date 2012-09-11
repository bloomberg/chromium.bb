// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_REQUESTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_REQUESTER_H_

#include <string>

#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"

namespace media_stream {

// MediaStreamRequester must be implemented by the class requesting a new media
// stream to be opened. MediaStreamManager will use this interface to signal
// success and error for a request.
class CONTENT_EXPORT MediaStreamRequester {
 public:
  // Called as a reply of a successful call to GenerateStream or
  // GenerateStreamForDevice.
  virtual void StreamGenerated(const std::string& label,
                               const StreamDeviceInfoArray& audio_devices,
                               const StreamDeviceInfoArray& video_devices) = 0;
  // Called if GenerateStream failed.
  virtual void StreamGenerationFailed(const std::string& label) = 0;
  // AudioDeviceFailed is called if an already opened audio device encounters
  // an error.
  virtual void AudioDeviceFailed(const std::string& label, int index) = 0;
  // VideoDeviceFailed is called if an already opened video device encounters
  // an error.
  virtual void VideoDeviceFailed(const std::string& label, int index) = 0;

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

}  // namespace media_stream

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_REQUESTER_H_
