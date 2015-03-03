// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_STATE_PROVIDER_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_STATE_PROVIDER_H_

#include "content/common/content_export.h"

namespace content {
class WebContents;
class AudioStreamMonitor;

// This class is associated with a WebContents, and maintains the audible
// state regarding all the players in it.
// The audible state is true if at least one player is playing a sound.
// Whenever the audible state of the WebContents as a whole changes, this
// class sends a notification to it.
//
// Each WebContentsImpl owns an AudioStateProvider
class CONTENT_EXPORT AudioStateProvider {
 public:
  explicit AudioStateProvider(WebContents* web_contents);
  virtual ~AudioStateProvider() {}

  // Indicates whether this service is available on the system.
  virtual bool IsAudioStateAvailable() const = 0;

  // If this provider uses monitoring (i.e. measure the signal),
  // return its monitor.
  virtual AudioStreamMonitor* audio_stream_monitor() = 0;

  // Returns true if the WebContents is playing or has recently been
  // playing the sound.
  virtual bool WasRecentlyAudible() const;

  void set_was_recently_audible_for_testing(bool value) {
    was_recently_audible_ = value;
  }

 protected:
  // Notify WebContents that the audio state has changed.
  void Notify(bool new_state);

  // The WebContents instance instance to receive indicator toggle
  // notifications.  This pointer should be valid for the lifetime of
  // AudioStreamMonitor.
  WebContents* const web_contents_;

  // The audio state that is being maintained
  bool  was_recently_audible_;

};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_STATE_PROVIDER_H_
