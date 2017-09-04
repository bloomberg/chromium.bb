// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_system_impl.h"

#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "media/base/bind_to_current_loop.h"

// Using base::Unretained for |&helper_| is safe since AudioSystem is deleted
// after audio thread is stopped.

// No need to bind the callback to the current loop if we are on the audio
// thread. However, the client still expects to receive the reply
// asynchronously, so we always post the helper function, which will
// syncronously call the (bound to current loop or not) callback. Thus the
// client always receives the callback on the thread it accesses AudioSystem on.

namespace media {

template <typename... Args>
inline base::OnceCallback<void(Args...)>
AudioSystemImpl::MaybeBindToCurrentLoop(
    base::OnceCallback<void(Args...)> callback) {
  return helper_.GetTaskRunner()->BelongsToCurrentThread()
             ? std::move(callback)
             : media::BindToCurrentLoop(std::move(callback));
}

AudioSystemImpl::AudioSystemImpl(AudioManager* audio_manager)
    : helper_(audio_manager) {
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
    OnAudioParamsCallback on_params_cb) {
  helper_.GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioSystemHelper::GetInputStreamParameters,
                     base::Unretained(&helper_), device_id,
                     MaybeBindToCurrentLoop(std::move(on_params_cb))));
}

void AudioSystemImpl::GetOutputStreamParameters(
    const std::string& device_id,
    OnAudioParamsCallback on_params_cb) {
  helper_.GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioSystemHelper::GetOutputStreamParameters,
                     base::Unretained(&helper_), device_id,
                     MaybeBindToCurrentLoop(std::move(on_params_cb))));
}

void AudioSystemImpl::HasInputDevices(OnBoolCallback on_has_devices_cb) {
  helper_.GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioSystemHelper::HasInputDevices,
                     base::Unretained(&helper_),
                     MaybeBindToCurrentLoop(std::move(on_has_devices_cb))));
}

void AudioSystemImpl::HasOutputDevices(OnBoolCallback on_has_devices_cb) {
  helper_.GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioSystemHelper::HasOutputDevices,
                     base::Unretained(&helper_),
                     MaybeBindToCurrentLoop(std::move(on_has_devices_cb))));
}

void AudioSystemImpl::GetDeviceDescriptions(
    bool for_input,
    OnDeviceDescriptionsCallback on_descriptions_cb) {
  helper_.GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioSystemHelper::GetDeviceDescriptions,
                     base::Unretained(&helper_), for_input,
                     MaybeBindToCurrentLoop(std::move(on_descriptions_cb))));
}

void AudioSystemImpl::GetAssociatedOutputDeviceID(
    const std::string& input_device_id,
    OnDeviceIdCallback on_device_id_cb) {
  helper_.GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioSystemHelper::GetAssociatedOutputDeviceID,
                     base::Unretained(&helper_), input_device_id,
                     MaybeBindToCurrentLoop(std::move(on_device_id_cb))));
}

void AudioSystemImpl::GetInputDeviceInfo(
    const std::string& input_device_id,
    OnInputDeviceInfoCallback on_input_device_info_cb) {
  helper_.GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AudioSystemHelper::GetInputDeviceInfo,
                                base::Unretained(&helper_), input_device_id,
                                MaybeBindToCurrentLoop(
                                    std::move(on_input_device_info_cb))));
}

}  // namespace media
