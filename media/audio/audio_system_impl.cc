// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_system_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/base/bind_to_current_loop.h"

// Using base::Unretained for |audio_manager_| is safe since it is deleted after
// its task runner, and AudioSystemImpl is deleted on the UI thread after the IO
// thread has been stopped and before |audio_manager_| deletion is scheduled.
namespace media {

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
    GetTaskRunner()->PostTask(FROM_HERE,
                              base::BindOnce(std::move(on_params_cb),
                                             GetInputParametersOnDeviceThread(
                                                 audio_manager_, device_id)));
    return;
  }
  base::PostTaskAndReplyWithResult(
      GetTaskRunner(), FROM_HERE,
      base::BindOnce(&AudioSystemImpl::GetInputParametersOnDeviceThread,
                     base::Unretained(audio_manager_), device_id),
      std::move(on_params_cb));
}

void AudioSystemImpl::GetOutputStreamParameters(
    const std::string& device_id,
    OnAudioParamsCallback on_params_cb) const {
  if (GetTaskRunner()->BelongsToCurrentThread()) {
    GetTaskRunner()->PostTask(FROM_HERE,
                              base::BindOnce(std::move(on_params_cb),
                                             GetOutputParametersOnDeviceThread(
                                                 audio_manager_, device_id)));
    return;
  }
  base::PostTaskAndReplyWithResult(
      GetTaskRunner(), FROM_HERE,
      base::BindOnce(&AudioSystemImpl::GetOutputParametersOnDeviceThread,
                     base::Unretained(audio_manager_), device_id),
      std::move(on_params_cb));
}

void AudioSystemImpl::HasInputDevices(OnBoolCallback on_has_devices_cb) const {
  if (GetTaskRunner()->BelongsToCurrentThread()) {
    GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_has_devices_cb),
                                  audio_manager_->HasAudioInputDevices()));
    return;
  }
  base::PostTaskAndReplyWithResult(
      GetTaskRunner(), FROM_HERE,
      base::BindOnce(&AudioManager::HasAudioInputDevices,
                     base::Unretained(audio_manager_)),
      std::move(on_has_devices_cb));
}

void AudioSystemImpl::HasOutputDevices(OnBoolCallback on_has_devices_cb) const {
  if (GetTaskRunner()->BelongsToCurrentThread()) {
    GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_has_devices_cb),
                                  audio_manager_->HasAudioOutputDevices()));
    return;
  }
  base::PostTaskAndReplyWithResult(
      GetTaskRunner(), FROM_HERE,
      base::BindOnce(&AudioManager::HasAudioOutputDevices,
                     base::Unretained(audio_manager_)),
      std::move(on_has_devices_cb));
}

void AudioSystemImpl::GetDeviceDescriptions(
    OnDeviceDescriptionsCallback on_descriptions_cb,
    bool for_input) {
  if (GetTaskRunner()->BelongsToCurrentThread()) {
    GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(on_descriptions_cb),
                       base::Passed(GetDeviceDescriptionsOnDeviceThread(
                           audio_manager_, for_input))));
    return;
  }

  base::PostTaskAndReplyWithResult(
      GetTaskRunner(), FROM_HERE,
      base::BindOnce(&AudioSystemImpl::GetDeviceDescriptionsOnDeviceThread,
                     base::Unretained(audio_manager_), for_input),
      std::move(on_descriptions_cb));
}

void AudioSystemImpl::GetAssociatedOutputDeviceID(
    const std::string& input_device_id,
    OnDeviceIdCallback on_device_id_cb) {
  if (GetTaskRunner()->BelongsToCurrentThread()) {
    GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_device_id_cb),
                                  audio_manager_->GetAssociatedOutputDeviceID(
                                      input_device_id)));
    return;
  }
  base::PostTaskAndReplyWithResult(
      GetTaskRunner(), FROM_HERE,
      base::BindOnce(&AudioManager::GetAssociatedOutputDeviceID,
                     base::Unretained(audio_manager_), input_device_id),
      std::move(on_device_id_cb));
}

void AudioSystemImpl::GetInputDeviceInfo(
    const std::string& input_device_id,
    OnInputDeviceInfoCallback on_input_device_info_cb) {
  // No need to bind |on_input_device_info_cb| to the current loop if we are on
  // the audio thread. However, the client still expect to receive the reply
  // asynchronously, so we always post GetInputDeviceInfoOnDeviceThread(), which
  // will syncronously call the (bound to current loop or not) callback.
  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioSystemImpl::GetInputDeviceInfoOnDeviceThread,
          base::Unretained(audio_manager_), input_device_id,
          GetTaskRunner()->BelongsToCurrentThread()
              ? std::move(on_input_device_info_cb)
              : media::BindToCurrentLoop(std::move(on_input_device_info_cb))));
}

base::SingleThreadTaskRunner* AudioSystemImpl::GetTaskRunner() const {
  return audio_manager_->GetTaskRunner();
}

// static
AudioParameters AudioSystemImpl::GetInputParametersOnDeviceThread(
    AudioManager* audio_manager,
    const std::string& device_id) {
  DCHECK(audio_manager->GetTaskRunner()->BelongsToCurrentThread());

  // TODO(olka): remove this when AudioManager::GetInputStreamParameters()
  // returns invalid parameters if the device is not found.
  if (AudioDeviceDescription::IsLoopbackDevice(device_id)) {
    // For system audio capture, we need an output device (namely speaker)
    // instead of an input device (namely microphone) to work.
    // AudioManager::GetInputStreamParameters will check |device_id| and
    // query the correct device for audio parameters by itself.
    if (!audio_manager->HasAudioOutputDevices())
      return AudioParameters();
  } else {
    if (!audio_manager->HasAudioInputDevices())
      return AudioParameters();
  }
  return audio_manager->GetInputStreamParameters(device_id);
}

// static
AudioParameters AudioSystemImpl::GetOutputParametersOnDeviceThread(
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

// static
AudioDeviceDescriptions AudioSystemImpl::GetDeviceDescriptionsOnDeviceThread(
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

// static
void AudioSystemImpl::GetInputDeviceInfoOnDeviceThread(
    AudioManager* audio_manager,
    const std::string& input_device_id,
    AudioSystem::OnInputDeviceInfoCallback on_input_device_info_cb) {
  DCHECK(audio_manager->GetTaskRunner()->BelongsToCurrentThread());
  const std::string associated_output_device_id =
      audio_manager->GetAssociatedOutputDeviceID(input_device_id);

  std::move(on_input_device_info_cb)
      .Run(GetInputParametersOnDeviceThread(audio_manager, input_device_id),
           associated_output_device_id.empty()
               ? AudioParameters()
               : GetOutputParametersOnDeviceThread(audio_manager,
                                                   associated_output_device_id),
           associated_output_device_id);
}

}  // namespace media
