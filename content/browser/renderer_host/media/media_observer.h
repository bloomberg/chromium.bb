// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_OBSERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_OBSERVER_H_
#pragma once

namespace media {
struct MediaLogEvent;
}

// A class may implement MediaObserver and register itself with ResourceContext
// to receive callbacks as media events occur.
class MediaObserver {
 public:
  virtual ~MediaObserver() {}

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
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_OBSERVER_H_
