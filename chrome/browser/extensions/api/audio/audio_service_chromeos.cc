// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/audio/audio_service.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

using api::audio::OutputDeviceInfo;
using api::audio::InputDeviceInfo;

class AudioServiceImpl : public AudioService,
                         public chromeos::CrasAudioHandler::AudioObserver {
 public:
  AudioServiceImpl();
  virtual ~AudioServiceImpl();

  // Called by listeners to this service to add/remove themselves as observers.
  virtual void AddObserver(AudioService::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(AudioService::Observer* observer) OVERRIDE;

  // Start to query audio device information.
  virtual void StartGetInfo(const GetInfoCallback& callback) OVERRIDE;
  virtual void SetActiveDevices(const DeviceIdList& device_list) OVERRIDE;
  virtual bool SetDeviceProperties(const std::string& device_id,
                                   bool muted,
                                   int volume,
                                   int gain) OVERRIDE;

 protected:
  // chromeos::CrasAudioHandler::AudioObserver overrides.
  virtual void OnOutputVolumeChanged() OVERRIDE;
  virtual void OnInputGainChanged() OVERRIDE;
  virtual void OnOutputMuteChanged() OVERRIDE;
  virtual void OnInputMuteChanged() OVERRIDE;
  virtual void OnAudioNodesChanged() OVERRIDE;
  virtual void OnActiveOutputNodeChanged() OVERRIDE;
  virtual void OnActiveInputNodeChanged() OVERRIDE;

 private:
  void NotifyDeviceChanged();

  bool FindDevice(uint64 id, chromeos::AudioDevice* device);
  uint64 GetIdFromStr(const std::string& id_str);

  // List of observers.
  ObserverList<AudioService::Observer> observer_list_;

  chromeos::CrasAudioHandler* cras_audio_handler_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<AudioServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioServiceImpl);
};

AudioServiceImpl::AudioServiceImpl()
    : cras_audio_handler_(NULL),
      weak_ptr_factory_(this) {
  if (chromeos::CrasAudioHandler::IsInitialized()) {
    cras_audio_handler_ = chromeos::CrasAudioHandler::Get();
    cras_audio_handler_->AddAudioObserver(this);
  }
}

AudioServiceImpl::~AudioServiceImpl() {
  if (cras_audio_handler_ && chromeos::CrasAudioHandler::IsInitialized()) {
    cras_audio_handler_->RemoveAudioObserver(this);
  }
}

void AudioServiceImpl::AddObserver(AudioService::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AudioServiceImpl::RemoveObserver(AudioService::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AudioServiceImpl::StartGetInfo(const GetInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(cras_audio_handler_);
  DCHECK(!callback.is_null());

  if (callback.is_null())
    return;

  OutputInfo output_info;
  InputInfo input_info;
  if (!cras_audio_handler_) {
    callback.Run(output_info, input_info, false);
    return;
  }

  chromeos::AudioDeviceList devices;
  cras_audio_handler_->GetAudioDevices(&devices);
  for (size_t i = 0; i < devices.size(); ++i) {
    if (!devices[i].is_input) {
      linked_ptr<OutputDeviceInfo> info(new OutputDeviceInfo());
      info->id = base::Uint64ToString(devices[i].id);
      info->name = devices[i].device_name + ": " + devices[i].display_name;
      info->is_active = devices[i].active;
      info->volume =
          cras_audio_handler_->GetOutputVolumePercentForDevice(devices[i].id);
      info->is_muted =
          cras_audio_handler_->IsOutputMutedForDevice(devices[i].id);
      output_info.push_back(info);
    } else {
      linked_ptr<InputDeviceInfo> info(new InputDeviceInfo());
      info->id = base::Uint64ToString(devices[i].id);
      info->name = devices[i].device_name + ": " + devices[i].display_name;
      info->is_active = devices[i].active;
      info->gain =
          cras_audio_handler_->GetInputGainPercentForDevice(devices[i].id);
      info->is_muted =
          cras_audio_handler_->IsInputMutedForDevice(devices[i].id);
      input_info.push_back(info);
    }
  }
  callback.Run(output_info, input_info, true);
}

void AudioServiceImpl::SetActiveDevices(const DeviceIdList& device_list) {
  DCHECK(cras_audio_handler_);
  if (!cras_audio_handler_)
    return;

  cras_audio_handler_->RemoveAllActiveNodes();

  for (size_t i = 0; i < device_list.size(); ++i) {
    chromeos::AudioDevice device;
    if (FindDevice(GetIdFromStr(device_list[i]), &device))
      cras_audio_handler_->AddActiveNode(device.id);
  }
}

bool AudioServiceImpl::SetDeviceProperties(const std::string& device_id,
                                           bool muted,
                                           int volume,
                                           int gain) {
  DCHECK(cras_audio_handler_);
  if (!cras_audio_handler_)
    return false;

  chromeos::AudioDevice device;
  bool found = FindDevice(GetIdFromStr(device_id), &device);
  if (!found)
    return false;

  cras_audio_handler_->SetMuteForDevice(device.id, muted);

  if (!device.is_input && volume != -1) {
    cras_audio_handler_->SetVolumeGainPercentForDevice(device.id, volume);
    return true;
  } else if (device.is_input && gain != -1) {
    cras_audio_handler_->SetVolumeGainPercentForDevice(device.id, gain);
    return true;
  }

  return false;
}

bool AudioServiceImpl::FindDevice(uint64 id, chromeos::AudioDevice* device) {
  chromeos::AudioDeviceList devices;
  cras_audio_handler_->GetAudioDevices(&devices);

  for (size_t i = 0; i < devices.size(); ++i) {
    if (devices[i].id == id) {
      *device = devices[i];
      return true;
    }
  }
  return false;
}

uint64 AudioServiceImpl::GetIdFromStr(const std::string& id_str) {
  uint64 device_id;
  if (!base::StringToUint64(id_str, &device_id))
    return 0;
  else
    return device_id;
}

void AudioServiceImpl::OnOutputVolumeChanged() {
  NotifyDeviceChanged();
}

void AudioServiceImpl::OnOutputMuteChanged() {
  NotifyDeviceChanged();
}

void AudioServiceImpl::OnInputGainChanged() {
  NotifyDeviceChanged();
}

void AudioServiceImpl::OnInputMuteChanged() {
  NotifyDeviceChanged();
}

void AudioServiceImpl::OnAudioNodesChanged() {
  NotifyDeviceChanged();
}

void AudioServiceImpl::OnActiveOutputNodeChanged() {
  NotifyDeviceChanged();
}

void AudioServiceImpl::OnActiveInputNodeChanged() {
  NotifyDeviceChanged();
}

void AudioServiceImpl::NotifyDeviceChanged() {
  FOR_EACH_OBSERVER(AudioService::Observer, observer_list_, OnDeviceChanged());
}

AudioService* AudioService::CreateInstance() {
  return new AudioServiceImpl;
}

}  // namespace extensions
