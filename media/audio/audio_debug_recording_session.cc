// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_debug_recording_session.h"

#include "base/files/file_path.h"
#include "media/audio/audio_debug_recording_session_impl.h"

namespace media {

AudioDebugRecordingSession::~AudioDebugRecordingSession() {}

std::unique_ptr<AudioDebugRecordingSession> AudioDebugRecordingSession::Create(
    const base::FilePath& debug_recording_file_path) {
  return std::make_unique<AudioDebugRecordingSessionImpl>(
      debug_recording_file_path);
}

}  // namespace media
