// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_OBSERVER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_OBSERVER_H_

#include "base/macros.h"
#include "content/common/content_export.h"

namespace media_session {
namespace mojom {
enum class AudioFocusType;
}  // namespace mojom
}  // namespace media_session

namespace content {

class MediaSession;

// The observer for observing audio focus events. This will not work on Android
// as it does not use the internal AudioFocusManager implementation.
class CONTENT_EXPORT AudioFocusObserver {
 public:
  AudioFocusObserver();
  ~AudioFocusObserver();

  // The given |MediaSession| gained audio focus with the specified type.
  virtual void OnFocusGained(MediaSession*,
                             media_session::mojom::AudioFocusType) = 0;

  // The given |NediaSession| lost audio focus.
  virtual void OnFocusLost(MediaSession*) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioFocusObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_OBSERVER_H_
