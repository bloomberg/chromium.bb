// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_OBSERVER_H_
#pragma once

#include "content/public/common/media_stream_request.h"

namespace media {
struct MediaLogEvent;
}

namespace content {

// An embedder may implement MediaObserver and return it from
// ContentBrowserClient to receive callbacks as media events occur.
class MediaObserver {
 public:
  // Called when an audio stream is deleted.
  virtual void OnDeleteAudioStream(void* host, int stream_id) = 0;

  // Called when an audio stream is set to playing or paused.
  virtual void OnSetAudioStreamPlaying(void* host, int stream_id,
                                       bool playing) = 0;

  // Called when the status of an audio stream is set to "created", "flushed",
  // "closed", or "error".
  virtual void OnSetAudioStreamStatus(void* host, int stream_id,
                                      const std::string& status) = 0;

  // Called when the volume of an audio stream is set.
  virtual void OnSetAudioStreamVolume(void* host, int stream_id,
                                      double volume) = 0;

  // Called when a MediaEvent occurs.
  virtual void OnMediaEvent(int render_process_id,
                            const media::MediaLogEvent& event) = 0;

  // Called when capture devices are opened.
  virtual void OnCaptureDevicesOpened(
      int render_process_id,
      int render_view_id,
      const content::MediaStreamDevices& devices) = 0;

  // Called when the opened capture devices are closed.
  virtual void OnCaptureDevicesClosed(
      int render_process_id,
      int render_view_id,
      const content::MediaStreamDevices& devices) = 0;

 protected:
  virtual ~MediaObserver() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_OBSERVER_H_
