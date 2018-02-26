// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_debug_recording_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"

namespace media {

namespace {

// Running id recording sources.
int g_next_stream_id = 1;

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

// Returns file name suffix created by appending |id| to |stream_type|.
base::FilePath GetDebugRecordingFileNameSuffix(
    const base::FilePath::StringType& stream_type,
    int id) {
  return base::FilePath(stream_type).AddExtension(IntToStringType(id));
}

}  // namespace

AudioDebugRecordingManager::AudioDebugRecordingManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)), weak_factory_(this) {}

AudioDebugRecordingManager::~AudioDebugRecordingManager() = default;

void AudioDebugRecordingManager::EnableDebugRecording(
    CreateWavFileCallback create_file_callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!create_file_callback.is_null());
  create_file_callback_ = std::move(create_file_callback);

  for (const auto& it : debug_recording_helpers_) {
    int id = it.first;
    AudioDebugRecordingHelper* recording_helper = it.second.first;
    const base::FilePath::StringType& stream_type = it.second.second;
    recording_helper->EnableDebugRecording(
        GetDebugRecordingFileNameSuffix(stream_type, id),
        create_file_callback_);
  }
}

void AudioDebugRecordingManager::DisableDebugRecording() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!create_file_callback_.is_null());
  for (const auto& it : debug_recording_helpers_) {
    AudioDebugRecordingHelper* recording_helper = it.second.first;
    recording_helper->DisableDebugRecording();
  }
  create_file_callback_.Reset();
}

std::unique_ptr<AudioDebugRecorder>
AudioDebugRecordingManager::RegisterDebugRecordingSource(
    const base::FilePath::StringType& stream_type,
    const AudioParameters& params) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  const int id = g_next_stream_id++;

  // Normally, the manager will outlive the one who registers and owns the
  // returned recorder. But to not require this we use a weak pointer.
  std::unique_ptr<AudioDebugRecordingHelper> recording_helper =
      CreateAudioDebugRecordingHelper(
          params, task_runner_,
          base::BindOnce(
              &AudioDebugRecordingManager::UnregisterDebugRecordingSource,
              weak_factory_.GetWeakPtr(), id));

  if (IsDebugRecordingEnabled()) {
    recording_helper->EnableDebugRecording(
        GetDebugRecordingFileNameSuffix(stream_type, id),
        create_file_callback_);
  }

  debug_recording_helpers_[id] =
      std::make_pair(recording_helper.get(), stream_type);

  return base::WrapUnique<AudioDebugRecorder>(recording_helper.release());
}

void AudioDebugRecordingManager::UnregisterDebugRecordingSource(int id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  auto it = debug_recording_helpers_.find(id);
  DCHECK(it != debug_recording_helpers_.end());
  debug_recording_helpers_.erase(id);
}

std::unique_ptr<AudioDebugRecordingHelper>
AudioDebugRecordingManager::CreateAudioDebugRecordingHelper(
    const AudioParameters& params,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::OnceClosure on_destruction_closure) {
  return std::make_unique<AudioDebugRecordingHelper>(
      params, task_runner, std::move(on_destruction_closure));
}

bool AudioDebugRecordingManager::IsDebugRecordingEnabled() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return !create_file_callback_.is_null();
}

}  // namespace media
