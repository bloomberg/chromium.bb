// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_DELEGATE_H_
#define CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_DELEGATE_H_

#include "content/browser/media/session/audio_focus_manager.h"

namespace media_session {
namespace mojom {
enum class AudioFocusType;
}  // namespace mojom
}  // namespace media_session

namespace content {

class MediaSessionImpl;

// AudioFocusDelegate is an interface abstracting audio focus handling for the
// MediaSession class.
class AudioFocusDelegate {
 public:
  // Factory method returning an implementation of AudioFocusDelegate.
  static std::unique_ptr<AudioFocusDelegate> Create(
      MediaSessionImpl* media_session);

  virtual ~AudioFocusDelegate() = default;

  virtual bool RequestAudioFocus(
      media_session::mojom::AudioFocusType audio_focus_type) = 0;
  virtual void AbandonAudioFocus() = 0;

  // Retrieves the current |AudioFocusType| for the associated |MediaSession|.
  virtual media_session::mojom::AudioFocusType GetCurrentFocusType() const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_DELEGATE_H_
