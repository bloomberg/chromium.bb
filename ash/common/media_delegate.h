// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_MEDIA_DELEGATE_H_
#define ASH_COMMON_MEDIA_DELEGATE_H_

#include "ash/public/cpp/session_types.h"

namespace ash {

enum MediaCaptureState {
  MEDIA_CAPTURE_NONE = 0,
  MEDIA_CAPTURE_AUDIO = 1 << 0,
  MEDIA_CAPTURE_VIDEO = 1 << 1,
  MEDIA_CAPTURE_AUDIO_VIDEO = MEDIA_CAPTURE_AUDIO | MEDIA_CAPTURE_VIDEO,
};

// A delegate class to control media playback.
class MediaDelegate {
 public:
  virtual ~MediaDelegate() {}

  // Handles the Next Track Media shortcut key.
  virtual void HandleMediaNextTrack() = 0;

  // Handles the Play/Pause Toggle Media shortcut key.
  virtual void HandleMediaPlayPause() = 0;

  // Handles the Previous Track Media shortcut key.
  virtual void HandleMediaPrevTrack() = 0;

  // Returns the current media recording state of web contents that belongs to
  // the the user @ |index|. See session_types.h for a description of UserIndex.
  virtual MediaCaptureState GetMediaCaptureState(UserIndex index) = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_MEDIA_DELEGATE_H_
