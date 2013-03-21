// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_OBSERVER_H_

#include "content/public/browser/media_request_state.h"
#include "content/public/common/media_stream_request.h"

namespace content {

// An embedder may implement MediaObserver and return it from
// ContentBrowserClient to receive callbacks as media events occur.
class MediaObserver {
 public:
  // Called when capture devices are opened. The observer can call
  // |close_callback| to stop the stream.
  virtual void OnCaptureDevicesOpened(
      int render_process_id,
      int render_view_id,
      const MediaStreamDevices& devices,
      const base::Closure& close_callback) = 0;

  // Called when the opened capture devices are closed.
  virtual void OnCaptureDevicesClosed(
      int render_process_id,
      int render_view_id,
      const MediaStreamDevices& devices) = 0;

  // Called when a audio capture device is plugged in or unplugged.
  virtual void OnAudioCaptureDevicesChanged(
      const MediaStreamDevices& devices) = 0;

  // Called when a video capture device is plugged in or unplugged.
  virtual void OnVideoCaptureDevicesChanged(
      const MediaStreamDevices& devices) = 0;

  // Called when a media request changes state.
  virtual void OnMediaRequestStateChanged(
      int render_process_id,
      int render_view_id,
      const MediaStreamDevice& device,
      MediaRequestState state) = 0;

  // Called when an audio stream is played or paused.
  virtual void OnAudioStreamPlayingChanged(
      int render_process_id,
      int render_view_id,
      int stream_id,
      bool playing) = 0;

 protected:
  virtual ~MediaObserver() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_OBSERVER_H_
