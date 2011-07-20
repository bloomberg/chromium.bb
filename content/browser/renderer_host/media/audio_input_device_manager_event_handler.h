// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioInputDeviceManagerEventHandler is used to signal events from
// AudioInoutDeviceManager when it's time to start and stop devices.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_EVENT_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_EVENT_HANDLER_H_

namespace media_stream {

class AudioInputDeviceManagerEventHandler {
 public:
  // Used to start the device referenced by session id and index.
  virtual void OnStartDevice(int session_id, int index) = 0;

  // Used to stop the device referenced by session id. This method is used
  // only when users call Close() without calling Stop() on a started device.
  virtual void OnStopDevice(int session_id) = 0;

  virtual ~AudioInputDeviceManagerEventHandler() {}
};

}  // namespace media_stream

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_EVENT_HANDLER_H_
