// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/audio/audio_service.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/audio_node.h"
#include "chromeos/dbus/cras_audio_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

using api::audio::OutputDeviceInfo;
using api::audio::InputDeviceInfo;

class AudioServiceImpl : public AudioService,
                         public chromeos::CrasAudioClient::Observer {
 public:
  AudioServiceImpl();
  virtual ~AudioServiceImpl();

  // Called by listeners to this service to add/remove themselves as observers.
  virtual void AddObserver(AudioService::Observer* observer);
  virtual void RemoveObserver(AudioService::Observer* observer);

  // Start to query audio device information.
  virtual void StartGetInfo(const GetInfoCallback& callback);

 protected:
  // chromeos::CrasAudioClient::Observer overrides.
  virtual void OutputVolumeChanged(int volume) OVERRIDE;
  virtual void OutputMuteChanged(bool mute_on) OVERRIDE;
  virtual void InputGainChanged(int gain) OVERRIDE;
  virtual void InputMuteChanged(bool mute_on) OVERRIDE;
  virtual void NodesChanged() OVERRIDE;
  virtual void ActiveOutputNodeChanged(uint64 node_id) OVERRIDE;
  virtual void ActiveInputNodeChanged(uint64 node_id) OVERRIDE;

 private:
  void NotifyDeviceChanged();

  // Callback for CrasAudioClient::GetNodes().
  void OnGetNodes(const GetInfoCallback& callback,
                  const chromeos::AudioNodeList& audio_nodes,
                  bool success);

  // List of observers.
  ObserverList<AudioService::Observer> observer_list_;

  chromeos::CrasAudioClient* cras_audio_client_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<AudioServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioServiceImpl);
};

AudioServiceImpl::AudioServiceImpl()
    : cras_audio_client_(NULL),
      weak_ptr_factory_(this) {
  if (chromeos::DBusThreadManager::IsInitialized() &&
      chromeos::DBusThreadManager::Get()) {
    cras_audio_client_ =
        chromeos::DBusThreadManager::Get()->GetCrasAudioClient();
    if (cras_audio_client_)
      cras_audio_client_->AddObserver(this);
  }
}

AudioServiceImpl::~AudioServiceImpl() {
  if (cras_audio_client_)
    cras_audio_client_->RemoveObserver(this);
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
  if (cras_audio_client_)
    cras_audio_client_->GetNodes(base::Bind(&AudioServiceImpl::OnGetNodes,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            callback));
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
        info->id = iter->id;
        info->name = iter->name;
        info->is_active = iter->active;
        output_info.push_back(info);
      } else {
        linked_ptr<InputDeviceInfo> info(new InputDeviceInfo());
        info->id = iter->id;
        info->name = iter->name;
        info->is_active = iter->active;
        input_info.push_back(info);
      }
    }
  }

  DCHECK(!callback.is_null());
  if (!callback.is_null())
    callback.Run(output_info, input_info, success);
}

void AudioServiceImpl::OutputVolumeChanged(int volume) {
  NotifyDeviceChanged();
}

void AudioServiceImpl::OutputMuteChanged(bool mute_on) {
  NotifyDeviceChanged();
}

void AudioServiceImpl::InputGainChanged(int gain) {
  NotifyDeviceChanged();
}

void AudioServiceImpl::InputMuteChanged(bool mute_on) {
  NotifyDeviceChanged();
}

void AudioServiceImpl::NodesChanged() {
  NotifyDeviceChanged();
}

void AudioServiceImpl::ActiveOutputNodeChanged(uint64 node_id) {
  NotifyDeviceChanged();
}

void AudioServiceImpl::ActiveInputNodeChanged(uint64 node_id) {
  NotifyDeviceChanged();
}

void AudioServiceImpl::NotifyDeviceChanged() {
  FOR_EACH_OBSERVER(AudioService::Observer, observer_list_, OnDeviceChanged());
}

AudioService* AudioService::CreateInstance() {
  return new AudioServiceImpl;
}

}  // namespace extensions
