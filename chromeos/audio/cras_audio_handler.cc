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
#include "chromeos/audio/mock_cras_audio_handler.h"
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
  g_cras_audio_handler = new MockCrasAudioHandler();
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

bool CrasAudioHandler::IsInputMuted() {
  return input_mute_on_;
}

int CrasAudioHandler::GetOutputVolumePercent() {
  return output_volume_;
}

int CrasAudioHandler::GetOutputVolumePercentForDevice(uint64 device_id) {
  if (device_id == active_output_node_id_)
    return output_volume_;
  else
    return (int) audio_pref_handler_->GetVolumeGainValue(device_id);
}

int CrasAudioHandler::GetInputGainPercent() {
  return input_gain_;
}

int CrasAudioHandler::GetInputGainPercentForDevice(uint64 device_id) {
  if (device_id == active_input_node_id_)
    return input_gain_;
  else
    return (int) audio_pref_handler_->GetVolumeGainValue(device_id);
}

uint64 CrasAudioHandler::GetActiveOutputNode() const {
  return active_output_node_id_;
}

uint64 CrasAudioHandler::GetActiveInputNode() const {
  return active_input_node_id_;
}

void CrasAudioHandler::GetAudioDevices(AudioDeviceList* device_list) const {
  for (size_t i = 0; i < audio_devices_.size(); ++i)
    device_list->push_back(audio_devices_[i]);
}

bool CrasAudioHandler::GetActiveOutputDevice(AudioDevice* device) const {
  for (size_t i = 0; i < audio_devices_.size(); ++i) {
    if (audio_devices_[i].id == active_output_node_id_) {
      *device = audio_devices_[i];
      return true;
    }
  }
  return false;
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
  SetOutputVolumeInternal(volume_percent);

  if (IsOutputMuted() && volume_percent > 0)
    SetOutputMute(false);
  if (!IsOutputMuted() && volume_percent == 0)
    SetOutputMute(true);
}

void CrasAudioHandler::SetInputGainPercent(int gain_percent) {
  gain_percent = min(max(gain_percent, 0), 100);
  if (gain_percent <= kMuteThresholdPercent)
    gain_percent = 0;
  SetInputGainInternal(gain_percent);

  if (IsInputMuted() && gain_percent > 0)
    SetInputMute(false);
  if (!IsInputMuted() && gain_percent == 0)
    SetInputMute(true);
}

void CrasAudioHandler::AdjustOutputVolumeByPercent(int adjust_by_percent) {
  SetOutputVolumePercent(output_volume_ + adjust_by_percent);
}

void CrasAudioHandler::SetOutputMute(bool mute_on) {
  if (output_mute_locked_)
    return;

  audio_pref_handler_->SetMuteValue(active_output_node_id_, mute_on);
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetOutputMute(mute_on);

  if (!mute_on) {
    if (output_volume_ <= kMuteThresholdPercent) {
      // Avoid the situation when sound has been unmuted, but the volume
      // is set to a very low value, so user still can't hear any sound.
      SetOutputVolumeInternal(kDefaultUnmuteVolumePercent);
    }
  }
}

void CrasAudioHandler::SetInputMute(bool mute_on) {
  if (input_mute_locked_)
    return;

  audio_pref_handler_->SetMuteValue(active_input_node_id_, mute_on);
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetInputMute(mute_on);
}

void CrasAudioHandler::SetActiveOutputNode(uint64 node_id) {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetActiveOutputNode(node_id);
}

void CrasAudioHandler::SetActiveInputNode(uint64 node_id) {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetActiveInputNode(node_id);
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

  audio_pref_handler_->SetVolumeGainValue(device_id, value);
}

void CrasAudioHandler::SetMuteForDevice(uint64 device_id, bool mute_on) {
  if (device_id == active_output_node_id_) {
    SetOutputMute(mute_on);
    return;
  } else if (device_id == active_input_node_id_) {
    SetInputMute(mute_on);
    return;
  }
  audio_pref_handler_->SetMuteValue(device_id, mute_on);
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
  if (!audio_pref_handler)
    return;
  // If the DBusThreadManager or the CrasAudioClient aren't available, there
  // isn't much we can do. This should only happen when running tests.
  if (!chromeos::DBusThreadManager::IsInitialized() ||
      !chromeos::DBusThreadManager::Get() ||
      !chromeos::DBusThreadManager::Get()->GetCrasAudioClient())
    return;
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->AddObserver(this);
  audio_pref_handler_->AddAudioPrefObserver(this);
  GetNodes();
}

CrasAudioHandler::~CrasAudioHandler() {
  if (!chromeos::DBusThreadManager::IsInitialized() ||
      !chromeos::DBusThreadManager::Get() ||
      !chromeos::DBusThreadManager::Get()->GetCrasAudioClient())
    return;
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      RemoveObserver(this);
  if (audio_pref_handler_)
    audio_pref_handler_->RemoveAudioPrefObserver(this);
  audio_pref_handler_ = NULL;
}

void CrasAudioHandler::AudioClientRestarted() {
  GetNodes();
}

void CrasAudioHandler::OutputVolumeChanged(int volume) {
  if (output_volume_ == volume)
    return;

  output_volume_ = volume;
  audio_pref_handler_->SetVolumeGainValue(active_output_node_id_, volume);
  FOR_EACH_OBSERVER(AudioObserver, observers_, OnOutputVolumeChanged());
}

void CrasAudioHandler::InputGainChanged(int gain) {
  if (input_gain_ == gain)
    return;

  input_gain_ = gain;
  audio_pref_handler_->SetVolumeGainValue(active_input_node_id_, gain);
  FOR_EACH_OBSERVER(AudioObserver, observers_, OnInputGainChanged());
}

void CrasAudioHandler::OutputMuteChanged(bool mute_on) {
  if (output_mute_on_ == mute_on)
    return;

  output_mute_on_ = mute_on;
  // TODO(rkc,jennyz): We need to save the mute preferences here. See
  // crbug.com/239646.
  FOR_EACH_OBSERVER(AudioObserver, observers_, OnOutputMuteChanged());
}

void CrasAudioHandler::InputMuteChanged(bool mute_on) {
  if (input_mute_on_ == mute_on)
    return;

  input_mute_on_ = mute_on;
  // TODO(rkc,jennyz): Fix this also when fixing the output mute. See
  // crbug.com/239646.
  FOR_EACH_OBSERVER(AudioObserver, observers_, OnInputMuteChanged());
}

void CrasAudioHandler::NodesChanged() {
  // Refresh audio nodes data.
  GetNodes();
}

void CrasAudioHandler::ActiveOutputNodeChanged(uint64 node_id) {
  if (active_output_node_id_ == node_id)
    return;

  active_output_node_id_ = node_id;
  SetupAudioOutputState();
  FOR_EACH_OBSERVER(AudioObserver, observers_, OnActiveOutputNodeChanged());
}

void CrasAudioHandler::ActiveInputNodeChanged(uint64 node_id) {
  if (active_input_node_id_ == node_id)
    return;

  active_input_node_id_ = node_id;
  SetupAudioInputState();
  FOR_EACH_OBSERVER(AudioObserver, observers_, OnActiveInputNodeChanged());
}

void CrasAudioHandler::OnAudioPolicyPrefChanged() {
  ApplyAudioPolicy();
}

void CrasAudioHandler::SetupAudioInputState() {
  ApplyAudioPolicy();

  // Set the initial audio state to the ones read from audio prefs.
  if (active_input_node_id_) {
    input_mute_on_ = audio_pref_handler_->GetMuteValue(active_input_node_id_);
    input_gain_ = audio_pref_handler_->GetVolumeGainValue(
        active_input_node_id_);
    SetInputMute(input_mute_on_);
    SetInputGainInternal(input_gain_);
  } else {
    SetInputMute(kPrefMuteOff);
    SetInputGainInternal(kDefaultVolumeGainPercent);
  }
}

void CrasAudioHandler::SetupAudioOutputState() {
  ApplyAudioPolicy();

  if (active_output_node_id_) {
    output_mute_on_ = audio_pref_handler_->GetMuteValue(active_output_node_id_);
    output_volume_ = audio_pref_handler_->GetVolumeGainValue(
        active_output_node_id_);
    SetOutputMute(output_mute_on_);
    SetOutputVolumeInternal(output_volume_);
  } else {
    SetOutputMute(kPrefMuteOff);
    SetOutputVolumeInternal(kDefaultVolumeGainPercent);
  }
}

void CrasAudioHandler::ApplyAudioPolicy() {
  output_mute_locked_ = false;
  if (!audio_pref_handler_->GetAudioOutputAllowedValue()) {
    SetOutputMute(true);
    output_mute_locked_ = true;
  }

  input_mute_locked_ = false;
  if (audio_pref_handler_->GetAudioCaptureAllowedValue()) {
    SetInputMute(false);
  } else {
    SetInputMute(true);
    input_mute_locked_ = true;
  }
}

void CrasAudioHandler::SetOutputVolumeInternal(int volume) {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetOutputVolume(volume);
}

void CrasAudioHandler::SetInputGainInternal(int gain) {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetInputGain(gain);
}

void CrasAudioHandler::GetNodes() {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->GetNodes(
      base::Bind(&CrasAudioHandler::HandleGetNodes,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::SwitchToDevice(const AudioDevice& device) {
  // The flow we follow is this,
  // .) Global mute.
  // .) Switch to active device.
  // .) Once device is switched, set sound state for new device.
  // We do this since during the state from when a device is plugged in or out,
  // we are in between devices. We cannot switch to the new device with the
  // old devices volume, since in certain situations it might be a very jarring
  // or disturbing sound (for example, plugging out headphones, which were set
  // to high volume, and switching to speakers, that were set to low volume).
  // To avoid this, we mute all sound, do our switch, and then directly set
  // the volume and mute to that of the new device. This way the user never has
  // to hear the wrong volume for a device.
  LOG(INFO) << "Switching active device to: " << device.ToString();
  if (device.is_input) {
    DBusThreadManager::Get()->GetCrasAudioClient()->SetInputMute(true);
    DBusThreadManager::Get()->GetCrasAudioClient()->SetActiveInputNode(
        device.id);
  } else {
    DBusThreadManager::Get()->GetCrasAudioClient()->SetOutputMute(true);
    DBusThreadManager::Get()->GetCrasAudioClient()->SetActiveOutputNode(
        device.id);
  }
}

void CrasAudioHandler::UpdateDevicesAndSwitchActive(
    const AudioNodeList& nodes) {
  bool input_device_removed = false;
  bool output_device_removed = false;

  size_t num_previous_input_devices = input_devices_pq_.size();
  size_t num_previous_output_devices = output_devices_pq_.size();

  audio_devices_.clear();
  active_input_node_id_ = 0;
  active_output_node_id_ = 0;
  has_alternative_input_ = false;
  has_alternative_output_ = false;

  while (!input_devices_pq_.empty())
    input_devices_pq_.pop();
  while (!output_devices_pq_.empty())
    output_devices_pq_.pop();

  for (size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].is_input && nodes[i].active)
      active_input_node_id_ = nodes[i].id;
    else if (!nodes[i].is_input && nodes[i].active)
      active_output_node_id_ = nodes[i].id;
    AudioDevice device(nodes[i]);
    audio_devices_.push_back(device);

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

  if (num_previous_input_devices > input_devices_pq_.size())
    input_device_removed = true;
  if (num_previous_output_devices > output_devices_pq_.size())
    output_device_removed = true;

  // If either,
  // .) the top input/output device is already active, or,
  // .) an input/output device was removed but not the active device,
  // then we don't need to switch the device, otherwise we do need to switch.
  if (!input_devices_pq_.empty() &&
      (!(input_devices_pq_.top().active || (input_device_removed &&
          active_input_node_id_))))
    SwitchToDevice(input_devices_pq_.top());
  else
    SetupAudioInputState();

  if (!output_devices_pq_.empty() &&
      (!(output_devices_pq_.top().active || (output_device_removed &&
          active_output_node_id_))))
    SwitchToDevice(output_devices_pq_.top());
  else
    SetupAudioOutputState();
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
