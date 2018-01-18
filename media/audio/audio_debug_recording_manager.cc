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

// Helper function that returns |base_file_name| with |file_name_extension| and
// |id| added to it as as extensions.
base::FilePath GetDebugRecordingFileNameWithExtensions(
    const base::FilePath& base_file_name,
    const base::FilePath::StringType& file_name_extension,
    int id) {
  return base_file_name.AddExtension(file_name_extension)
      .AddExtension(IntToStringType(id));
}

void CreateFile(const base::FilePath& file_name,
                base::OnceCallback<void(base::File)> reply_callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          [](const base::FilePath& file_name) {
            return base::File(file_name, base::File::FLAG_CREATE_ALWAYS |
                                             base::File::FLAG_WRITE);
          },
          file_name),
      std::move(reply_callback));
}

}  // namespace

AudioDebugRecordingManager::AudioDebugRecordingManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)), weak_factory_(this) {}

AudioDebugRecordingManager::~AudioDebugRecordingManager() = default;

void AudioDebugRecordingManager::EnableDebugRecording(
    const base::FilePath& base_file_name) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!base_file_name.empty());

  debug_recording_base_file_name_ = base_file_name;
  for (const auto& it : debug_recording_helpers_) {
    int id = it.first;
    AudioDebugRecordingHelper* recording_helper = it.second.first;
    const base::FilePath::StringType& file_name_extension = it.second.second;
    recording_helper->EnableDebugRecording(
        GetDebugRecordingFileNameWithExtensions(debug_recording_base_file_name_,
                                                file_name_extension, id));
  }
}

void AudioDebugRecordingManager::DisableDebugRecording() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  for (const auto& it : debug_recording_helpers_) {
    AudioDebugRecordingHelper* recording_helper = it.second.first;
    recording_helper->DisableDebugRecording();
  }
  debug_recording_base_file_name_.clear();
}

std::unique_ptr<AudioDebugRecorder>
AudioDebugRecordingManager::RegisterDebugRecordingSource(
    const base::FilePath::StringType& file_name_extension,
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
        GetDebugRecordingFileNameWithExtensions(debug_recording_base_file_name_,
                                                file_name_extension, id));
  }

  debug_recording_helpers_[id] =
      std::make_pair(recording_helper.get(), file_name_extension);

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
      params, task_runner, base::BindRepeating(&CreateFile),
      std::move(on_destruction_closure));
}

bool AudioDebugRecordingManager::IsDebugRecordingEnabled() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return !debug_recording_base_file_name_.empty();
}

}  // namespace media
