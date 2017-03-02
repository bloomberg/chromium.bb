// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_debug_recording_helper.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "media/audio/audio_debug_file_writer.h"

namespace media {

AudioDebugRecordingHelper::AudioDebugRecordingHelper(
    const AudioParameters& params,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    base::OnceClosure on_destruction_closure)
    : params_(params),
      recording_enabled_(0),
      task_runner_(std::move(task_runner)),
      file_task_runner_(std::move(file_task_runner)),
      on_destruction_closure_(std::move(on_destruction_closure)),
      weak_factory_(this) {}

AudioDebugRecordingHelper::~AudioDebugRecordingHelper() {
  if (on_destruction_closure_)
    std::move(on_destruction_closure_).Run();
}

void AudioDebugRecordingHelper::EnableDebugRecording(
    const base::FilePath& file_name) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!debug_writer_);
  DCHECK(!file_name.empty());

  debug_writer_ = CreateAudioDebugFileWriter(params_, file_task_runner_);
  debug_writer_->Start(
      file_name.AddExtension(debug_writer_->GetFileNameExtension()));

  base::subtle::NoBarrier_Store(&recording_enabled_, 1);
}

void AudioDebugRecordingHelper::DisableDebugRecording() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::subtle::NoBarrier_Store(&recording_enabled_, 0);

  if (debug_writer_) {
    debug_writer_->Stop();
    debug_writer_.reset();
  }
}

void AudioDebugRecordingHelper::OnData(const AudioBus* source) {
  // Check if debug recording is enabled to avoid an unecessary copy and thread
  // jump if not. Recording can be disabled between the atomic Load() here and
  // DoWrite(), but it's fine with a single unnecessary copy+jump at disable
  // time. We use an atomic operation for accessing the flag on different
  // threads. No memory barrier is needed for the same reason; a race is no
  // problem at enable and disable time. Missing one buffer of data doesn't
  // matter.
  base::subtle::Atomic32 recording_enabled =
      base::subtle::NoBarrier_Load(&recording_enabled_);
  if (!recording_enabled)
    return;

  // TODO(grunell) Don't create a new AudioBus each time. Maybe a pool of
  // AudioBuses. See also comment in
  // AudioInputController::AudioCallback::PerformOptionalDebugRecording.
  std::unique_ptr<AudioBus> audio_bus_copy =
      AudioBus::Create(source->channels(), source->frames());
  source->CopyTo(audio_bus_copy.get());

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AudioDebugRecordingHelper::DoWrite,
                 weak_factory_.GetWeakPtr(), base::Passed(&audio_bus_copy)));
}

void AudioDebugRecordingHelper::DoWrite(std::unique_ptr<media::AudioBus> data) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (debug_writer_)
    debug_writer_->Write(std::move(data));
}

std::unique_ptr<AudioDebugFileWriter>
AudioDebugRecordingHelper::CreateAudioDebugFileWriter(
    const AudioParameters& params,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) {
  return base::MakeUnique<AudioDebugFileWriter>(params, file_task_runner);
}

}  // namespace media
