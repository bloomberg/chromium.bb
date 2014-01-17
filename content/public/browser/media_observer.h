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
      int page_request_id,
      const GURL& security_origin,
      const MediaStreamDevice& device,
      MediaRequestState state) = 0;

  // Called when an audio stream transitions into a playing or paused state, and
  // also at regular intervals to report the current power level of the audio
  // signal in dBFS (decibels relative to full-scale) units.  |clipped| is true
  // if any part of the audio signal has been clipped since the last call.  See
  // media/audio/audio_power_monitor.h for more info.
  virtual void OnAudioStreamPlayingChanged(
      int render_process_id,
      int render_view_id,
      int stream_id,
      bool is_playing,
      float power_dbfs,
      bool clipped) = 0;

  // Called when the audio stream is being created.
  virtual void OnCreatingAudioStream(int render_process_id,
                                     int render_view_id) = 0;

 protected:
  virtual ~MediaObserver() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_OBSERVER_H_
