// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_debug_recording_session_impl.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/single_thread_task_runner.h"
#include "media/audio/audio_manager.h"

namespace media {

// Posting AudioManager::Get() as unretained is safe because
// AudioManager::Shutdown() (called before AudioManager destruction) shuts down
// AudioManager thread.

AudioDebugRecordingSessionImpl::AudioDebugRecordingSessionImpl(
    const base::FilePath& debug_recording_file_path) {
  AudioManager* audio_manager = AudioManager::Get();
  if (audio_manager == nullptr)
    return;

  audio_manager->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AudioManager::EnableDebugRecording,
                                base::Unretained(audio_manager),
                                debug_recording_file_path));
}

AudioDebugRecordingSessionImpl::~AudioDebugRecordingSessionImpl() {
  AudioManager* audio_manager = AudioManager::Get();
  if (audio_manager == nullptr)
    return;

  audio_manager->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AudioManager::DisableDebugRecording,
                                base::Unretained(audio_manager)));
}

}  // namespace media
