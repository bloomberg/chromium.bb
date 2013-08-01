// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/cras_audio_handler.h"

#include <algorithm>
#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "chromeos/audio/audio_devices_pref_handler.h"
#include "chromeos/audio/audio_devices_pref_handler_stub.h"
#include "chromeos/dbus/dbus_thread_manager.h"

using std::max;
using std::min;

namespace chromeos {

namespace {

// Default value for unmuting, as a percent in the range [0, 100].
// Used when sound is unmuted, but volume was less than kMuteThresholdPercent.
const int kDefaultUnmuteVolumePercent = 4;

// Volume value which should be considered as muted in range [0, 100].
const int kMuteThresholdPercent = 1;

static CrasAudioHandler* g_cras_audio_handler = NULL;

}  // namespace

CrasAudioHandler::AudioObserver::AudioObserver() {
}

CrasAudioHandler::AudioObserver::~AudioObserver() {
}

void CrasAudioHandler::AudioObserver::OnOutputVolumeChanged() {
}

void CrasAudioHandler::AudioObserver::OnInputGainChanged() {
}

void CrasAudioHandler::AudioObserver::OnOutputMuteChanged() {
}

void CrasAudioHandler::AudioObserver::OnInputMuteChanged() {
}

void CrasAudioHandler::AudioObserver::OnAudioNodesChanged() {
}

void CrasAudioHandler::AudioObserver::OnActiveOutputNodeChanged() {
}

void CrasAudioHandler::AudioObserver::OnActiveInputNodeChanged() {
}

// static
void CrasAudioHandler::Initialize(
    scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler) {
  CHECK(!g_cras_audio_handler);
  g_cras_audio_handler = new CrasAudioHandler(audio_pref_handler);
}

// static
void CrasAudioHandler::InitializeForTesting() {
  CHECK(!g_cras_audio_handler);
  CrasAudioHandler::Initialize(new AudioDevicesPrefHandlerStub());
}

// static
void CrasAudioHandler::Shutdown() {
  CHECK(g_cras_audio_handler);
  delete g_cras_audio_handler;
  g_cras_audio_handler = NULL;
}

// static
bool CrasAudioHandler::IsInitialized() {
  return g_cras_audio_handler != NULL;
}

// static
CrasAudioHandler* CrasAudioHandler::Get() {
  CHECK(g_cras_audio_handler)
      << "CrasAudioHandler::Get() called before Initialize().";
  return g_cras_audio_handler;
}

void CrasAudioHandler::AddAudioObserver(AudioObserver* observer) {
  observers_.AddObserver(observer);
}

void CrasAudioHandler::RemoveAudioObserver(AudioObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool CrasAudioHandler::IsOutputMuted() {
  return output_mute_on_;
}

bool CrasAudioHandler::IsOutputMutedForDevice(uint64 device_id) {
  const AudioDevice* device = GetDeviceFromId(device_id);
  if (!device)
    return false;
  return audio_pref_handler_->GetMuteValue(*device);
}

bool CrasAudioHandler::IsOutputVolumeBelowDefaultMuteLvel() {
  return output_volume_ <= kMuteThresholdPercent;
}

bool CrasAudioHandler::IsInputMuted() {
  return input_mute_on_;
}

bool CrasAudioHandler::IsInputMutedForDevice(uint64 device_id) {
  const AudioDevice* device = GetDeviceFromId(device_id);
  if (!device)
    return false;
  return audio_pref_handler_->GetMuteValue(*device);
}

int CrasAudioHandler::GetOutputVolumePercent() {
  return output_volume_;
}

int CrasAudioHandler::GetOutputVolumePercentForDevice(uint64 device_id) {
  if (device_id == active_output_node_id_) {
    return output_volume_;
  } else {
    const AudioDevice* device = GetDeviceFromId(device_id);
    if (!device)
      return kDefaultVolumeGainPercent;
    return static_cast<int>(audio_pref_handler_->GetVolumeGainValue(*device));
  }
}

int CrasAudioHandler::GetInputGainPercent() {
  return input_gain_;
}

int CrasAudioHandler::GetInputGainPercentForDevice(uint64 device_id) {
  if (device_id == active_input_node_id_) {
    return input_gain_;
  } else {
    const AudioDevice* device = GetDeviceFromId(device_id);
    if (!device)
      return kDefaultVolumeGainPercent;
    return static_cast<int>(audio_pref_handler_->GetVolumeGainValue(*device));
  }
}

uint64 CrasAudioHandler::GetActiveOutputNode() const {
  return active_output_node_id_;
}

uint64 CrasAudioHandler::GetActiveInputNode() const {
  return active_input_node_id_;
}

void CrasAudioHandler::GetAudioDevices(AudioDeviceList* device_list) const {
  device_list->clear();
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end(); ++it)
    device_list->push_back(it->second);
}

bool CrasAudioHandler::GetActiveOutputDevice(AudioDevice* device) const {
  const AudioDevice* active_device = GetDeviceFromId(active_output_node_id_);
  if (!active_device || !device)
    return false;
  *device = *active_device;
  return true;
}

bool CrasAudioHandler::has_alternative_input() const {
  return has_alternative_input_;
}

bool CrasAudioHandler::has_alternative_output() const {
  return has_alternative_output_;
}

void CrasAudioHandler::SetOutputVolumePercent(int volume_percent) {
  volume_percent = min(max(volume_percent, 0), 100);
  if (volume_percent <= kMuteThresholdPercent)
    volume_percent = 0;
  output_volume_ = volume_percent;

  if (const AudioDevice* device = GetDeviceFromId(active_output_node_id_))
    audio_pref_handler_->SetVolumeGainValue(*device, output_volume_);

  SetOutputNodeVolume(active_output_node_id_, output_volume_);
  FOR_EACH_OBSERVER(AudioObserver, observers_, OnOutputVolumeChanged());
}

void CrasAudioHandler::SetInputGainPercent(int gain_percent) {
  gain_percent = min(max(gain_percent, 0), 100);
  if (gain_percent <= kMuteThresholdPercent)
    gain_percent = 0;
  input_gain_ = gain_percent;

  if (const AudioDevice* device = GetDeviceFromId(active_input_node_id_))
    audio_pref_handler_->SetVolumeGainValue(*device, input_gain_);

  SetInputNodeGain(active_input_node_id_, input_gain_);
  FOR_EACH_OBSERVER(AudioObserver, observers_, OnInputGainChanged());
}

void CrasAudioHandler::AdjustOutputVolumeByPercent(int adjust_by_percent) {
  SetOutputVolumePercent(output_volume_ + adjust_by_percent);
}

void CrasAudioHandler::SetOutputMute(bool mute_on) {
  if (!SetOutputMuteInternal(mute_on))
    return;

  if (const AudioDevice* device = GetDeviceFromId(active_output_node_id_))
    audio_pref_handler_->SetMuteValue(*device, output_mute_on_);

  FOR_EACH_OBSERVER(AudioObserver, observers_, OnOutputMuteChanged());
}

void CrasAudioHandler::AdjustOutputVolumeToAudibleLevel() {
  if (output_volume_ <= kMuteThresholdPercent) {
    // Avoid the situation when sound has been unmuted, but the volume
    // is set to a very low value, so user still can't hear any sound.
    SetOutputVolumePercent(kDefaultUnmuteVolumePercent);
  }
}

void CrasAudioHandler::SetInputMute(bool mute_on) {
  if (!SetInputMuteInternal(mute_on))
    return;

  AudioDevice device;
  if (const AudioDevice* device = GetDeviceFromId(active_input_node_id_))
    audio_pref_handler_->SetMuteValue(*device, input_mute_on_);


  FOR_EACH_OBSERVER(AudioObserver, observers_, OnInputMuteChanged());
}

void CrasAudioHandler::SetActiveOutputNode(uint64 node_id) {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetActiveOutputNode(node_id);
  FOR_EACH_OBSERVER(AudioObserver, observers_, OnActiveOutputNodeChanged());
}

void CrasAudioHandler::SetActiveInputNode(uint64 node_id) {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetActiveInputNode(node_id);
  FOR_EACH_OBSERVER(AudioObserver, observers_, OnActiveInputNodeChanged());
}

void CrasAudioHandler::SetVolumeGainPercentForDevice(uint64 device_id,
                                                     int value) {
  if (device_id == active_output_node_id_) {
    SetOutputVolumePercent(value);
    return;
  } else if (device_id == active_input_node_id_) {
    SetInputGainPercent(value);
    return;
  }

  value = min(max(value, 0), 100);
  if (value <= kMuteThresholdPercent)
    value = 0;

  if (const AudioDevice* device = GetDeviceFromId(device_id))
    audio_pref_handler_->SetVolumeGainValue(*device, value);
}

void CrasAudioHandler::SetMuteForDevice(uint64 device_id, bool mute_on) {
  if (device_id == active_output_node_id_) {
    SetOutputMute(mute_on);
    return;
  } else if (device_id == active_input_node_id_) {
    SetInputMute(mute_on);
    return;
  }

  AudioDevice device;
  if (const AudioDevice* device = GetDeviceFromId(device_id))
    audio_pref_handler_->SetMuteValue(*device, mute_on);
}

CrasAudioHandler::CrasAudioHandler(
    scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler)
    : audio_pref_handler_(audio_pref_handler),
      weak_ptr_factory_(this),
      output_mute_on_(false),
      input_mute_on_(false),
      output_volume_(0),
      input_gain_(0),
      active_output_node_id_(0),
      active_input_node_id_(0),
      has_alternative_input_(false),
      has_alternative_output_(false),
      output_mute_locked_(false),
      input_mute_locked_(false) {
  if (!audio_pref_handler.get())
    return;
  // If the DBusThreadManager or the CrasAudioClient aren't available, there
  // isn't much we can do. This should only happen when running tests.
  if (!chromeos::DBusThreadManager::IsInitialized() ||
      !chromeos::DBusThreadManager::Get() ||
      !chromeos::DBusThreadManager::Get()->GetCrasAudioClient())
    return;
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->AddObserver(this);
  audio_pref_handler_->AddAudioPrefObserver(this);
  InitializeAudioState();
}

CrasAudioHandler::~CrasAudioHandler() {
  if (!chromeos::DBusThreadManager::IsInitialized() ||
      !chromeos::DBusThreadManager::Get() ||
      !chromeos::DBusThreadManager::Get()->GetCrasAudioClient())
    return;
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      RemoveObserver(this);
  if (audio_pref_handler_.get())
    audio_pref_handler_->RemoveAudioPrefObserver(this);
  audio_pref_handler_ = NULL;
}

void CrasAudioHandler::AudioClientRestarted() {
  InitializeAudioState();
}

void CrasAudioHandler::OutputMuteChanged(bool mute_on) {
  if (output_mute_on_ != mute_on) {
    LOG(WARNING) << "output mute state inconsistent, internal mute="
        << output_mute_on_  << ", dbus signal mute=" << mute_on;
  }
}

void CrasAudioHandler::InputMuteChanged(bool mute_on) {
  if (input_mute_on_ != mute_on) {
    LOG(WARNING) << "input mute state inconsistent, internal mute="
        << input_mute_on_  << ", dbus signal mute=" << mute_on;
  }
}

void CrasAudioHandler::NodesChanged() {
  // Refresh audio nodes data.
  GetNodes();
}

void CrasAudioHandler::ActiveOutputNodeChanged(uint64 node_id) {
  if (active_output_node_id_ == node_id)
    return;

  // Active audio output device should always be changed by chrome.
  LOG(WARNING) << "Active output node changed unexpectedly by system node_id="
      << "0x" << std::hex << node_id;
}

void CrasAudioHandler::ActiveInputNodeChanged(uint64 node_id) {
  if (active_input_node_id_ == node_id)
    return;

  // Active audio input device should always be changed by chrome.
  LOG(WARNING) << "Active input node changed unexpectedly by system node_id="
      << "0x" << std::hex << node_id;
}

void CrasAudioHandler::OnAudioPolicyPrefChanged() {
  ApplyAudioPolicy();
}

const AudioDevice* CrasAudioHandler::GetDeviceFromId(uint64 device_id) const {
  AudioDeviceMap::const_iterator it = audio_devices_.find(device_id);
  if (it == audio_devices_.end())
    return NULL;

  return &(it->second);
}

void CrasAudioHandler::SetupAudioInputState() {
  // Set the initial audio state to the ones read from audio prefs.
  const AudioDevice* device = GetDeviceFromId(active_input_node_id_);
  if (!device) {
    LOG(ERROR) << "Can't set up audio state for unknow input device id ="
        << "0x" << std::hex << active_input_node_id_;
    return;
  }
  input_mute_on_ = audio_pref_handler_->GetMuteValue(*device);
  input_gain_ = audio_pref_handler_->GetVolumeGainValue(*device);
  SetInputMuteInternal(input_mute_on_);
  SetInputNodeGain(active_input_node_id_, input_gain_);
}

void CrasAudioHandler::SetupAudioOutputState() {
  const AudioDevice* device = GetDeviceFromId(active_output_node_id_);
  if (!device) {
    LOG(ERROR) << "Can't set up audio state for unknow output device id ="
               << "0x" << std::hex << active_output_node_id_;
    return;
  }
  output_mute_on_ = audio_pref_handler_->GetMuteValue(*device);
  output_volume_ = audio_pref_handler_->GetVolumeGainValue(*device);
  SetOutputMuteInternal(output_mute_on_);
  SetOutputNodeVolume(active_output_node_id_, output_volume_);
}

void CrasAudioHandler::InitializeAudioState() {
  ApplyAudioPolicy();
  GetNodes();
}

void CrasAudioHandler::ApplyAudioPolicy() {
  output_mute_locked_ = false;
  if (!audio_pref_handler_->GetAudioOutputAllowedValue()) {
    // Mute the device, but do not update the preference.
    SetOutputMuteInternal(true);
    output_mute_locked_ = true;
  } else {
    // Restore the mute state.
    const AudioDevice* device = GetDeviceFromId(active_output_node_id_);
    if (device)
      SetOutputMuteInternal(audio_pref_handler_->GetMuteValue(*device));
  }

  input_mute_locked_ = false;
  if (audio_pref_handler_->GetAudioCaptureAllowedValue()) {
    SetInputMute(false);
  } else {
    SetInputMute(true);
    input_mute_locked_ = true;
  }
}

void CrasAudioHandler::SetOutputNodeVolume(uint64 node_id, int volume) {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetOutputNodeVolume(node_id, volume);
}

bool  CrasAudioHandler::SetOutputMuteInternal(bool mute_on) {
  if (output_mute_locked_)
    return false;

  output_mute_on_ = mute_on;
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetOutputUserMute(mute_on);
  return true;
}

void CrasAudioHandler::SetInputNodeGain(uint64 node_id, int gain) {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetInputNodeGain(node_id, gain);
}

bool CrasAudioHandler::SetInputMuteInternal(bool mute_on) {
  if (input_mute_locked_)
    return false;

  input_mute_on_ = mute_on;
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetInputMute(mute_on);
  return true;
}

void CrasAudioHandler::GetNodes() {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->GetNodes(
      base::Bind(&CrasAudioHandler::HandleGetNodes,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool CrasAudioHandler::ChangeActiveDevice(const AudioDevice& new_active_device,
                                          uint64* current_active_node_id) {
  // If the device we want to switch to is already the current active device,
  // do nothing.
  if (new_active_device.active &&
      new_active_device.id == *current_active_node_id) {
    return false;
  }
  if (GetDeviceFromId(*current_active_node_id))
    audio_devices_[*current_active_node_id].active = false;
  *current_active_node_id = new_active_device.id;
  audio_devices_[*current_active_node_id].active = true;
  return true;
}

void CrasAudioHandler::SwitchToDevice(const AudioDevice& device) {
  if (device.is_input) {
    if (!ChangeActiveDevice(device, &active_input_node_id_))
      return;
    SetupAudioInputState();
    SetActiveInputNode(active_input_node_id_);
  } else {
    if (!ChangeActiveDevice(device, &active_output_node_id_))
      return;
    SetupAudioOutputState();
    SetActiveOutputNode(active_output_node_id_);
  }
}

void CrasAudioHandler::UpdateDevicesAndSwitchActive(
    const AudioNodeList& nodes) {
  audio_devices_.clear();
  has_alternative_input_ = false;
  has_alternative_output_ = false;

  while (!input_devices_pq_.empty())
    input_devices_pq_.pop();
  while (!output_devices_pq_.empty())
    output_devices_pq_.pop();

  for (size_t i = 0; i < nodes.size(); ++i) {
    AudioDevice device(nodes[i]);
    audio_devices_[device.id] = device;

    if (!has_alternative_input_ &&
        device.is_input &&
        device.type != AUDIO_TYPE_INTERNAL_MIC) {
      has_alternative_input_ = true;
    } else if (!has_alternative_output_ &&
               !device.is_input &&
               device.type != AUDIO_TYPE_INTERNAL_SPEAKER) {
      has_alternative_output_ = true;
    }

    if (device.is_input)
      input_devices_pq_.push(device);
    else
      output_devices_pq_.push(device);
  }

  if (!input_devices_pq_.empty())
    SwitchToDevice(input_devices_pq_.top());

  if (!output_devices_pq_.empty())
    SwitchToDevice(output_devices_pq_.top());
}

void CrasAudioHandler::HandleGetNodes(const chromeos::AudioNodeList& node_list,
                                      bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to retrieve audio nodes data";
    return;
  }

  UpdateDevicesAndSwitchActive(node_list);
  FOR_EACH_OBSERVER(AudioObserver, observers_, OnAudioNodesChanged());
}

}  // namespace chromeos
