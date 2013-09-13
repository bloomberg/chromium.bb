// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/audio/audio_service.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/audio_node.h"
#include "chromeos/dbus/cras_audio_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
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

  // Callback for CrasAudioClient::GetNodes().
  void OnGetNodes(const GetInfoCallback& callback,
                  const chromeos::AudioNodeList& audio_nodes,
                  bool success);

  // ErrorCallback for CrasAudioClient::GetNodes().
  void OnGetNodesError(const std::string& error_name,
                       const std::string& error_msg);

  bool FindDevice(uint64 id, chromeos::AudioDevice* device);
  uint64 GetIdFromStr(const std::string& id_str);

  // List of observers.
  ObserverList<AudioService::Observer> observer_list_;

  chromeos::CrasAudioClient* cras_audio_client_;
  chromeos::CrasAudioHandler* cras_audio_handler_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<AudioServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioServiceImpl);
};

AudioServiceImpl::AudioServiceImpl()
    : cras_audio_client_(NULL),
      cras_audio_handler_(NULL),
      weak_ptr_factory_(this) {
  if (chromeos::DBusThreadManager::IsInitialized() &&
      chromeos::DBusThreadManager::Get()) {
    cras_audio_client_ =
        chromeos::DBusThreadManager::Get()->GetCrasAudioClient();
    if (chromeos::CrasAudioHandler::IsInitialized()) {
      cras_audio_handler_ = chromeos::CrasAudioHandler::Get();
      cras_audio_handler_->AddAudioObserver(this);
    }
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(cras_audio_client_);
  // TODO(jennyz,rkc): Replace cras_audio_client_ call with CrasAudioHandler
  // Api call.
  if (cras_audio_client_)
    cras_audio_client_->GetNodes(base::Bind(&AudioServiceImpl::OnGetNodes,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            callback),
                                 base::Bind(&AudioServiceImpl::OnGetNodesError,
                                            weak_ptr_factory_.GetWeakPtr()));
}

void AudioServiceImpl::SetActiveDevices(const DeviceIdList& device_list) {
  DCHECK(cras_audio_handler_);
  if (!cras_audio_handler_)
    return;

  bool input_device_set = false;
  bool output_device_set = false;

  for (size_t i = 0; i < device_list.size(); ++i) {
    chromeos::AudioDevice device;
    bool found = FindDevice(GetIdFromStr(device_list[i]), &device);
    if (found) {
      if (device.is_input && !input_device_set) {
        cras_audio_handler_->SwitchToDevice(device);
        input_device_set = true;
      } else if (!device.is_input && !output_device_set) {
        cras_audio_handler_->SwitchToDevice(device);
        output_device_set = true;
      }
    }
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

void AudioServiceImpl::OnGetNodes(const GetInfoCallback& callback,
                                  const chromeos::AudioNodeList& audio_nodes,
                                  bool success) {
  OutputInfo output_info;
  InputInfo input_info;
  if (success) {
    for (chromeos::AudioNodeList::const_iterator iter = audio_nodes.begin();
        iter != audio_nodes.end(); ++iter) {
      if (!iter->is_input) {
        linked_ptr<OutputDeviceInfo> info(new OutputDeviceInfo());
        info->id = base::Uint64ToString(iter->id);
        info->name = iter->device_name + ": " + iter->name;
        info->is_active = iter->active;
        info->volume = cras_audio_handler_->GetOutputVolumePercentForDevice(
            iter->id);
        info->is_muted = cras_audio_handler_->IsOutputMutedForDevice(iter->id);
        output_info.push_back(info);
      } else {
        linked_ptr<InputDeviceInfo> info(new InputDeviceInfo());
        info->id = base::Uint64ToString(iter->id);
        info->name = iter->device_name + ": " + iter->name;
        info->is_active = iter->active;
        info->gain = cras_audio_handler_->GetInputGainPercentForDevice(
            iter->id);
        info->is_muted = cras_audio_handler_->IsInputMutedForDevice(iter->id);
        input_info.push_back(info);
      }
    }
  }

  DCHECK(!callback.is_null());
  if (!callback.is_null())
    callback.Run(output_info, input_info, success);
}

void AudioServiceImpl::OnGetNodesError(const std::string& error_name,
                                       const std::string& error_msg) {
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
