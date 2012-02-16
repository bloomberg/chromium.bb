// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_EVENTHANDLER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_EVENTHANDLER_H_

#include <string>

#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"

class CONTENT_EXPORT MediaStreamDispatcherEventHandler {
 public:
  // A new media stream have been created.
  virtual void OnStreamGenerated(
      int request_id,
      const std::string& label,
      const media_stream::StreamDeviceInfoArray& audio_device_array,
      const media_stream::StreamDeviceInfoArray& video_device_array) = 0;

  // Creation of a new media stream failed. The user might have denied access
  // to the requested devices or no device is available.
  virtual void OnStreamGenerationFailed(int request_id) = 0;

  // An error have occurred on a video device. This is called if a runtime
  // error occurs.
  virtual void OnVideoDeviceFailed(const std::string& label,
                                   int index) = 0;

  // An error have occurred on an audio device. This is called if a runtime
  // error occurs.
  virtual void OnAudioDeviceFailed(const std::string& label,
                                   int index) = 0;

  // A new list of devices have been enumerated.
  virtual void OnDevicesEnumerated(
      int request_id,
      const media_stream::StreamDeviceInfoArray& device_array) = 0;

  // Failed to enumerate devices.
  virtual void OnDevicesEnumerationFailed(int request_id) = 0;

  // A device has been opened.
  virtual void OnDeviceOpened(
      int request_id,
      const std::string& label,
      const media_stream::StreamDeviceInfo& device_info) = 0;

  // Failed to open the device.
  virtual void OnDeviceOpenFailed(int request_id) = 0;

 protected:
  virtual ~MediaStreamDispatcherEventHandler() {}
};

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_EVENTHANDLER_H_
