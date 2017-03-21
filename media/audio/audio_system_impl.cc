// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_system_impl.h"

#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "media/audio/audio_device_description.h"
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
  // returns invalid parameters if the device is not found.
  if (!audio_manager->HasAudioInputDevices())
    return AudioParameters();

  return audio_manager->GetInputStreamParameters(device_id);
}

AudioParameters GetOutputParametersOnDeviceThread(
    AudioManager* audio_manager,
    const std::string& device_id) {
  DCHECK(audio_manager->GetTaskRunner()->BelongsToCurrentThread());

  // TODO(olka): remove this when
  // AudioManager::Get[Default]OutputStreamParameters() returns invalid
  // parameters if the device is not found.
  if (!audio_manager->HasAudioOutputDevices())
    return AudioParameters();

  return media::AudioDeviceDescription::IsDefaultDevice(device_id)
             ? audio_manager->GetDefaultOutputStreamParameters()
             : audio_manager->GetOutputStreamParameters(device_id);
}

AudioDeviceDescriptions GetDeviceDescriptionsOnDeviceThread(
    AudioManager* audio_manager,
    bool for_input) {
  DCHECK(audio_manager->GetTaskRunner()->BelongsToCurrentThread());
  AudioDeviceDescriptions descriptions;
  if (for_input)
    audio_manager->GetAudioInputDeviceDescriptions(&descriptions);
  else
    audio_manager->GetAudioOutputDeviceDescriptions(&descriptions);
  return descriptions;
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

void AudioSystemImpl::GetOutputStreamParameters(
    const std::string& device_id,
    OnAudioParamsCallback on_params_cb) const {
  if (GetTaskRunner()->BelongsToCurrentThread()) {
    GetTaskRunner()->PostTask(
        FROM_HERE, base::Bind(on_params_cb, GetOutputParametersOnDeviceThread(
                                                audio_manager_, device_id)));
    return;
  }
  base::PostTaskAndReplyWithResult(
      GetTaskRunner(), FROM_HERE,
      base::Bind(&GetOutputParametersOnDeviceThread,
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

void AudioSystemImpl::GetDeviceDescriptions(
    OnDeviceDescriptionsCallback on_descriptions_cp,
    bool for_input) {
  if (GetTaskRunner()->BelongsToCurrentThread()) {
    GetTaskRunner()->PostTask(
        FROM_HERE, base::Bind(on_descriptions_cp,
                              base::Passed(GetDeviceDescriptionsOnDeviceThread(
                                  audio_manager_, for_input))));
    return;
  }

  base::PostTaskAndReplyWithResult(
      GetTaskRunner(), FROM_HERE,
      base::Bind(&GetDeviceDescriptionsOnDeviceThread,
                 base::Unretained(audio_manager_), for_input),
      std::move(on_descriptions_cp));
}

AudioManager* AudioSystemImpl::GetAudioManager() const {
  return audio_manager_;
}

base::SingleThreadTaskRunner* AudioSystemImpl::GetTaskRunner() const {
  return audio_manager_->GetTaskRunner();
}

}  // namespace media
