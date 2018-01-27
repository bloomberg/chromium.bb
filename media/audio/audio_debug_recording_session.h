// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_SESSION_H_
#define MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_SESSION_H_

#include <memory>

#include "base/macros.h"
#include "media/base/media_export.h"

namespace base {
class FilePath;
}

namespace media {

// Creating/Destroying an AudioDebugRecordingSession object enables/disables
// audio debug recording.
class MEDIA_EXPORT AudioDebugRecordingSession {
 public:
  virtual ~AudioDebugRecordingSession();
  static std::unique_ptr<AudioDebugRecordingSession> Create(
      const base::FilePath& debug_recording_file_path);

 protected:
  AudioDebugRecordingSession() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDebugRecordingSession);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_SESSION_H_
