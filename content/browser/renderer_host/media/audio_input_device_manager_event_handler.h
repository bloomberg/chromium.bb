// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioInputDeviceManagerEventHandler is used to signal events from
// AudioInoutDeviceManager when it's time to start and stop devices.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_EVENT_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_EVENT_HANDLER_H_

#include "content/common/content_export.h"

namespace media_stream {

class CONTENT_EXPORT AudioInputDeviceManagerEventHandler {
 public:
  // Called by AudioInputDeviceManager to create an audio stream using the
  // device index when the device has been started.
  virtual void OnDeviceStarted(int session_id,
                               const std::string& device_id) = 0;

  // Called by AudioInputDeviceManager to stop the audio stream when a device
  // has been stopped. This method is used only when users call Close() without
  // calling Stop() on a started device.
  virtual void OnDeviceStopped(int session_id) = 0;

  virtual ~AudioInputDeviceManagerEventHandler() {}
};

}  // namespace media_stream

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_EVENT_HANDLER_H_
