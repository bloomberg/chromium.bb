// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_input_stream.h"
#include <memory>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_bus.h"
#include "media/base/media_switches.h"

namespace media {

namespace {
base::subtle::AtomicWord g_fake_input_streams_are_muted = 0;
}

AudioInputStream* FakeAudioInputStream::MakeFakeStream(
    AudioManagerBase* manager,
    const AudioParameters& params) {
  return new FakeAudioInputStream(manager, params);
}

FakeAudioInputStream::FakeAudioInputStream(AudioManagerBase* manager,
                                           const AudioParameters& params)
    : audio_manager_(manager),
      callback_(NULL),
      capture_thread_("FakeAudioInput"),
      params_(params),
      audio_bus_(AudioBus::Create(params)) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
}

FakeAudioInputStream::~FakeAudioInputStream() {
  DCHECK(!callback_);
}

bool FakeAudioInputStream::Open() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  audio_bus_->Zero();

  return true;
}

void FakeAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(callback);
  DCHECK(!callback_);
  DCHECK(!fake_audio_worker_);

  callback_ = callback;

  base::Thread::Options options;
  options.priority = base::ThreadPriority::REALTIME_AUDIO;
  CHECK(capture_thread_.StartWithOptions(options));

  fake_audio_worker_ =
      std::make_unique<FakeAudioWorker>(capture_thread_.task_runner(), params_);

  fake_audio_worker_->Start(base::BindRepeating(
      &FakeAudioInputStream::ReadAudioFromSource, base::Unretained(this)));
}

void FakeAudioInputStream::Stop() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  // Start was never called
  if (!callback_) {
    return;
  }

  DCHECK(capture_thread_.IsRunning());
  DCHECK(fake_audio_worker_);

  fake_audio_worker_->Stop();
  fake_audio_worker_.reset();
  capture_thread_.Stop();

  callback_ = NULL;
}

void FakeAudioInputStream::Close() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  Stop();
  audio_manager_->ReleaseInputStream(this);
}

double FakeAudioInputStream::GetMaxVolume() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return 1.0;
}

void FakeAudioInputStream::SetVolume(double volume) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
}

double FakeAudioInputStream::GetVolume() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return 1.0;
}

bool FakeAudioInputStream::IsMuted() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return base::subtle::NoBarrier_Load(&g_fake_input_streams_are_muted) != 0;
}

bool FakeAudioInputStream::SetAutomaticGainControl(bool enabled) {
  return false;
}

bool FakeAudioInputStream::GetAutomaticGainControl() {
  return false;
}

void FakeAudioInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  // Not supported. Do nothing.
}

void FakeAudioInputStream::ReadAudioFromSource(base::TimeTicks ideal_time,
                                               base::TimeTicks now) {
  DCHECK(capture_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(callback_);

  if (!audio_source_)
    audio_source_ = ChooseSource();

  // This OnMoreData()/OnData() timing would never happen in a real system:
  //
  //   1. Real AudioSources would never be asked to generate audio that should
  //      already be playing-out exactly at this very moment; they are asked to
  //      do so for audio to be played-out in the future.
  //   2. Real AudioInputStreams could never provide audio that is striking a
  //      microphone element exactly at this very moment; they provide audio
  //      that happened in the recent past.
  //
  // However, it would be pointless to add a FIFO queue here to delay the signal
  // in this "fake" implementation. So, just hack the timing and carry-on.
  audio_source_->OnMoreData(base::TimeDelta(), ideal_time, 0, audio_bus_.get());
  callback_->OnData(audio_bus_.get(), ideal_time, 1.0);
}

using AudioSourceCallback = AudioOutputStream::AudioSourceCallback;
std::unique_ptr<AudioSourceCallback> FakeAudioInputStream::ChooseSource() {
  DCHECK(capture_thread_.task_runner()->BelongsToCurrentThread());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFileForFakeAudioCapture)) {
    base::CommandLine::StringType switch_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
            switches::kUseFileForFakeAudioCapture);
    base::CommandLine::StringVector parameters =
        base::SplitString(switch_value, FILE_PATH_LITERAL("%"),
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    CHECK(parameters.size() > 0) << "You must pass <file>[%noloop] to  --"
                                 << switches::kUseFileForFakeAudioCapture
                                 << ".";
    base::FilePath path_to_wav_file = base::FilePath(parameters[0]);
    bool looping = true;
    if (parameters.size() == 2) {
      CHECK(parameters[1] == FILE_PATH_LITERAL("noloop"))
          << "Unknown parameter " << parameters[1] << " to "
          << switches::kUseFileForFakeAudioCapture << ".";
      looping = false;
    }
    return std::make_unique<FileSource>(params_, path_to_wav_file, looping);
  }
  return std::make_unique<BeepingSource>(params_);
}

void FakeAudioInputStream::BeepOnce() {
  BeepingSource::BeepOnce();
}

void FakeAudioInputStream::SetGlobalMutedState(bool is_muted) {
  base::subtle::NoBarrier_Store(&g_fake_input_streams_are_muted,
                                (is_muted ? 1 : 0));
}

}  // namespace media
