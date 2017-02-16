// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_system_impl.h"

#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "media/audio/audio_manager.h"

// Using base::Unretained for |audio_manager_| is safe since it is deleted after
// its task runner, and AudioSystemImpl is deleted on the UI thread after the IO
// thread has been stopped and before |audio_manager_| deletion is scheduled.
namespace media {

namespace {

AudioParameters GetInputParametersOnDeviceThread(AudioManager* audio_manager,
                                                 const std::string& device_id) {
  DCHECK(audio_manager->GetTaskRunner()->BelongsToCurrentThread());

  // TODO(olka): remove this when AudioManager::GetInputStreamParameters()
  // works this way on all the platforms.
  if (!audio_manager->HasAudioInputDevices())
    return AudioParameters();

  return audio_manager->GetInputStreamParameters(device_id);
}

}  // namespace

AudioSystemImpl::AudioSystemImpl(AudioManager* audio_manager)
    : audio_manager_(audio_manager) {
  DCHECK(audio_manager_);
  AudioSystem::SetInstance(this);
}

AudioSystemImpl::~AudioSystemImpl() {
  AudioSystem::ClearInstance(this);
}

// static
std::unique_ptr<AudioSystem> AudioSystemImpl::Create(
    AudioManager* audio_manager) {
  return base::WrapUnique(new AudioSystemImpl(audio_manager));
}

void AudioSystemImpl::GetInputStreamParameters(
    const std::string& device_id,
    OnAudioParamsCallback on_params_cb) const {
  if (GetTaskRunner()->BelongsToCurrentThread()) {
    GetTaskRunner()->PostTask(
        FROM_HERE, base::Bind(on_params_cb, GetInputParametersOnDeviceThread(
                                                audio_manager_, device_id)));
    return;
  }
  base::PostTaskAndReplyWithResult(
      GetTaskRunner(), FROM_HERE,
      base::Bind(&GetInputParametersOnDeviceThread,
                 base::Unretained(audio_manager_), device_id),
      std::move(on_params_cb));
}

void AudioSystemImpl::HasInputDevices(OnBoolCallback on_has_devices_cb) const {
  if (GetTaskRunner()->BelongsToCurrentThread()) {
    GetTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(on_has_devices_cb, audio_manager_->HasAudioInputDevices()));
    return;
  }
  base::PostTaskAndReplyWithResult(
      GetTaskRunner(), FROM_HERE,
      base::Bind(&AudioManager::HasAudioInputDevices,
                 base::Unretained(audio_manager_)),
      std::move(on_has_devices_cb));
}

AudioManager* AudioSystemImpl::GetAudioManager() const {
  return audio_manager_;
}

base::SingleThreadTaskRunner* AudioSystemImpl::GetTaskRunner() const {
  return audio_manager_->GetTaskRunner();
}

}  // namespace media
