// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mock_audio_manager.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_parameters.h"

namespace media {

void MockAudioManager::Deleter::operator()(
    const MockAudioManager* instance) const {
  CHECK(instance);
  if (instance->GetTaskRunner()->BelongsToCurrentThread()) {
    delete instance;
    return;
  }
  // AudioManager must be destroyed on the audio thread.
  if (!instance->GetTaskRunner()->DeleteSoon(FROM_HERE, instance)) {
    LOG(WARNING) << "Failed to delete AudioManager instance.";
  }
}

MockAudioManager::MockAudioManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : AudioManager(task_runner, task_runner) {}

MockAudioManager::~MockAudioManager() {
}

bool MockAudioManager::HasAudioOutputDevices() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return true;
}

bool MockAudioManager::HasAudioInputDevices() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return has_input_devices_;
}

base::string16 MockAudioManager::GetAudioInputDeviceModel() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return base::string16();
}

void MockAudioManager::ShowAudioInputSettings() {
}

void MockAudioManager::GetAudioInputDeviceDescriptions(
    AudioDeviceDescriptions* device_descriptions) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
}

void MockAudioManager::GetAudioOutputDeviceDescriptions(
    AudioDeviceDescriptions* device_descriptions) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
}

media::AudioOutputStream* MockAudioManager::MakeAudioOutputStream(
    const media::AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return NULL;
}

media::AudioOutputStream* MockAudioManager::MakeAudioOutputStreamProxy(
    const media::AudioParameters& params,
    const std::string& device_id) {
  NOTREACHED();
  return NULL;
}

media::AudioInputStream* MockAudioManager::MakeAudioInputStream(
    const media::AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return NULL;
}

void MockAudioManager::AddOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
}

void MockAudioManager::RemoveOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
}

AudioParameters MockAudioManager::GetDefaultOutputStreamParameters() {
  return AudioParameters();
}

AudioParameters MockAudioManager::GetOutputStreamParameters(
      const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return AudioParameters();
}

AudioParameters MockAudioManager::GetInputStreamParameters(
    const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return input_params_;
}

std::string MockAudioManager::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return std::string();
}

std::unique_ptr<AudioLog> MockAudioManager::CreateAudioLog(
    AudioLogFactory::AudioComponent component) {
  return nullptr;
}

void MockAudioManager::InitializeOutputDebugRecording(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) {}

void MockAudioManager::EnableOutputDebugRecording(
    const base::FilePath& base_file_name) {}

void MockAudioManager::DisableOutputDebugRecording() {}

const char* MockAudioManager::GetName() {
  return nullptr;
}

void MockAudioManager::SetInputStreamParameters(
    const AudioParameters& input_params) {
  input_params_ = input_params;
}

void MockAudioManager::SetHasInputDevices(bool has_input_devices) {
  has_input_devices_ = has_input_devices;
}

}  // namespace media.
