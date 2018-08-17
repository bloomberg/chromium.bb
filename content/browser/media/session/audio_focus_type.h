// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_TYPE_H_
#define CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_TYPE_H_

namespace content {

// The different types of audio focus a MediaSession can request.
enum class AudioFocusType {
  // When a media session requests the Gain audio focus type the following will
  // happen:
  //  - any existing media sessions will be suspended.
  //  - any pepper media sessions will be ducked.
  // In most cases media sessions should request this type.
  Gain,

  // When a media session requests the GainTransientMayDuck audio focus type it
  // will duck any existing media sessions. This is useful for short lived audio
  // that needs to play over existing audio e.g. notifications.
  GainTransientMayDuck,
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_TYPE_H_
