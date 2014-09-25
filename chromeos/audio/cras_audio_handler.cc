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

bool IsSameAudioDevice(const AudioDevice& a, const AudioDevice& b) {
  return a.id == b.id && a.is_input == b.is_input && a.type == b.type
      && a.device_name == b.device_name;
}

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

bool CrasAudioHandler::HasKeyboardMic() {
  return GetKeyboardMic() != NULL;
}

bool CrasAudioHandler::IsOutputMuted() {
  return output_mute_on_;
}

bool CrasAudioHandler::IsOutputMutedForDevice(uint64 device_id) {
  const AudioDevice* device = GetDeviceFromId(device_id);
  if (!device)
    return false;
  DCHECK(!device->is_input);
  return audio_pref_handler_->GetMuteValue(*device);
}

bool CrasAudioHandler::IsOutputVolumeBelowDefaultMuteLevel() {
  return output_volume_ <= kMuteThresholdPercent;
}

bool CrasAudioHandler::IsInputMuted() {
  return input_mute_on_;
}

bool CrasAudioHandler::IsInputMutedForDevice(uint64 device_id) {
  const AudioDevice* device = GetDeviceFromId(device_id);
  if (!device)
    return false;
  DCHECK(device->is_input);
  // We don't record input mute state for each device in the prefs,
  // for any non-active input device, we assume mute is off.
  if (device->id == active_input_node_id_)
    return input_mute_on_;
  return false;
}

int CrasAudioHandler::GetOutputDefaultVolumeMuteThreshold() {
  return kMuteThresholdPercent;
}

int CrasAudioHandler::GetOutputVolumePercent() {
  return output_volume_;
}

int CrasAudioHandler::GetOutputVolumePercentForDevice(uint64 device_id) {
  if (device_id == active_output_node_id_) {
    return output_volume_;
  } else {
    const AudioDevice* device = GetDeviceFromId(device_id);
    return static_cast<int>(audio_pref_handler_->GetOutputVolumeValue(device));
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
    return static_cast<int>(audio_pref_handler_->GetInputGainValue(device));
  }
}

uint64 CrasAudioHandler::GetPrimaryActiveOutputNode() const {
  return active_output_node_id_;
}

uint64 CrasAudioHandler::GetPrimaryActiveInputNode() const {
  return active_input_node_id_;
}

void CrasAudioHandler::GetAudioDevices(AudioDeviceList* device_list) const {
  device_list->clear();
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end(); ++it)
    device_list->push_back(it->second);
}

bool CrasAudioHandler::GetPrimaryActiveOutputDevice(AudioDevice* device) const {
  const AudioDevice* active_device = GetDeviceFromId(active_output_node_id_);
  if (!active_device || !device)
    return false;
  *device = *active_device;
  return true;
}

void CrasAudioHandler::SetKeyboardMicActive(bool active) {
  const AudioDevice* keyboard_mic = GetKeyboardMic();
  if (!keyboard_mic)
    return;
  // Keyboard mic is invisible to chromeos users. It is always added or removed
  // as additional active node.
  DCHECK(active_input_node_id_ && active_input_node_id_ != keyboard_mic->id);
  if (active)
    AddActiveNode(keyboard_mic->id);
  else
    RemoveActiveNode(keyboard_mic->id);
}

void CrasAudioHandler::AddActiveNode(uint64 node_id) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device) {
    VLOG(1) << "AddActiveInputNode: Cannot find device id="
            << "0x" << std::hex << node_id;
    return;
  }

  // If there is no primary active device, set |node_id| to primary active node.
  if ((device->is_input && !active_input_node_id_) ||
      (!device->is_input && !active_output_node_id_)) {
    SwitchToDevice(*device);
    return;
  }
  AddAdditionalActiveNode(node_id);
}

void CrasAudioHandler::RemoveActiveNode(uint64 node_id) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device) {
    VLOG(1) << "RemoveActiveInputNode: Cannot find device id="
            << "0x" << std::hex << node_id;
    return;
  }

  // We do NOT allow to remove the primary active node.
  if (device->is_input && device->id == active_input_node_id_) {
    VLOG(1) << "Cannot remove the primary active input node: "
            << "0x" << std::hex << node_id;
    return;
  }
  if (!device->is_input && device->id == active_output_node_id_) {
    VLOG(1) << "Cannot remove the primary active output node: "
            << "0x" << std::hex << node_id;
    return;
  }
  RemoveActiveNodeInternal(node_id);
}

void CrasAudioHandler::RemoveAllActiveNodes() {
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end();
       ++it) {
    RemoveActiveNodeInternal(it->second.id);
  }
}

bool CrasAudioHandler::has_alternative_input() const {
  return has_alternative_input_;
}

bool CrasAudioHandler::has_alternative_output() const {
  return has_alternative_output_;
}

void CrasAudioHandler::SetOutputVolumePercent(int volume_percent) {
  // Set all active devices to the same volume.
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end();
       it++) {
    const AudioDevice& device = it->second;
    if (!device.is_input && device.active)
      SetOutputNodeVolumePercent(device.id, volume_percent);
  }
}

// TODO: Rename the 'Percent' to something more meaningful.
void CrasAudioHandler::SetInputGainPercent(int gain_percent) {
  // TODO(jennyz): Should we set all input devices' gain to the same level?
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end();
       it++) {
    const AudioDevice& device = it->second;
    if (device.is_input && device.active)
      SetInputNodeGainPercent(active_input_node_id_, gain_percent);
  }
}

void CrasAudioHandler::AdjustOutputVolumeByPercent(int adjust_by_percent) {
  SetOutputVolumePercent(output_volume_ + adjust_by_percent);
}

void CrasAudioHandler::SetOutputMute(bool mute_on) {
  if (!SetOutputMuteInternal(mute_on))
    return;

  // Save the mute state for all active output audio devices.
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end();
       it++) {
    const AudioDevice& device = it->second;
    if (!device.is_input && device.active) {
      audio_pref_handler_->SetMuteValue(device, output_mute_on_);
    }
  }

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
  const AudioDevice* device = GetDeviceFromId(device_id);
  if (!device)
    return;

  if (device->is_input)
    SetInputNodeGainPercent(device_id, value);
  else
    SetOutputNodeVolumePercent(device_id, value);
}

void CrasAudioHandler::SetMuteForDevice(uint64 device_id, bool mute_on) {
  if (device_id == active_output_node_id_) {
    SetOutputMute(mute_on);
    return;
  } else if (device_id == active_input_node_id_) {
    VLOG(1) << "SetMuteForDevice sets active input device id="
            << "0x" << std::hex << device_id << " mute=" << mute_on;
    SetInputMute(mute_on);
    return;
  }

  const AudioDevice* device = GetDeviceFromId(device_id);
  // Input device's mute state is not recorded in the pref. crbug.com/365050.
  if (device && !device->is_input)
    audio_pref_handler_->SetMuteValue(*device, mute_on);
}

void CrasAudioHandler::LogErrors() {
  log_errors_ = true;
}

CrasAudioHandler::CrasAudioHandler(
    scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler)
    : audio_pref_handler_(audio_pref_handler),
      output_mute_on_(false),
      input_mute_on_(false),
      output_volume_(0),
      input_gain_(0),
      active_output_node_id_(0),
      active_input_node_id_(0),
      has_alternative_input_(false),
      has_alternative_output_(false),
      output_mute_locked_(false),
      input_mute_locked_(false),
      log_errors_(false),
      weak_ptr_factory_(this) {
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
  if (chromeos::DBusThreadManager::Get()->GetSessionManagerClient()) {
    chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
        AddObserver(this);
  }
  InitializeAudioState();
}

CrasAudioHandler::~CrasAudioHandler() {
  if (!chromeos::DBusThreadManager::IsInitialized() ||
      !chromeos::DBusThreadManager::Get() ||
      !chromeos::DBusThreadManager::Get()->GetCrasAudioClient())
    return;
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      RemoveObserver(this);
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
      RemoveObserver(this);
  if (audio_pref_handler_.get())
    audio_pref_handler_->RemoveAudioPrefObserver(this);
  audio_pref_handler_ = NULL;
}

void CrasAudioHandler::AudioClientRestarted() {
  // Make sure the logging is enabled in case cras server
  // restarts after crashing.
  LogErrors();
  InitializeAudioState();
}

void CrasAudioHandler::NodesChanged() {
  // Refresh audio nodes data.
  GetNodes();
}

void CrasAudioHandler::ActiveOutputNodeChanged(uint64 node_id) {
  if (active_output_node_id_ == node_id)
    return;

  // Active audio output device should always be changed by chrome.
  // During system boot, cras may change active input to unknown device 0x1,
  // we don't need to log it, since it is not an valid device.
  if (GetDeviceFromId(node_id)) {
    LOG_IF(WARNING, log_errors_)
        << "Active output node changed unexpectedly by system node_id="
        << "0x" << std::hex << node_id;
  }
}

void CrasAudioHandler::ActiveInputNodeChanged(uint64 node_id) {
  if (active_input_node_id_ == node_id)
    return;

  // Active audio input device should always be changed by chrome.
  // During system boot, cras may change active input to unknown device 0x2,
  // we don't need to log it, since it is not an valid device.
  if (GetDeviceFromId(node_id)) {
    LOG_IF(WARNING, log_errors_)
        << "Active input node changed unexpectedly by system node_id="
        << "0x" << std::hex << node_id;
  }
}

void CrasAudioHandler::OnAudioPolicyPrefChanged() {
  ApplyAudioPolicy();
}

void CrasAudioHandler::EmitLoginPromptVisibleCalled() {
  // Enable logging after cras server is started, which will be after
  // EmitLoginPromptVisible.
  LogErrors();
}

const AudioDevice* CrasAudioHandler::GetDeviceFromId(uint64 device_id) const {
  AudioDeviceMap::const_iterator it = audio_devices_.find(device_id);
  if (it == audio_devices_.end())
    return NULL;

  return &(it->second);
}

const AudioDevice* CrasAudioHandler::GetKeyboardMic() const {
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end(); it++) {
    if (it->second.is_input && it->second.type == AUDIO_TYPE_KEYBOARD_MIC)
      return &(it->second);
  }
  return NULL;
}

void CrasAudioHandler::SetupAudioInputState() {
  // Set the initial audio state to the ones read from audio prefs.
  const AudioDevice* device = GetDeviceFromId(active_input_node_id_);
  if (!device) {
    LOG_IF(ERROR, log_errors_)
        << "Can't set up audio state for unknown input device id ="
        << "0x" << std::hex << active_input_node_id_;
    return;
  }
  input_gain_ = audio_pref_handler_->GetInputGainValue(device);
  VLOG(1) << "SetupAudioInputState for active device id="
          << "0x" << std::hex << device->id << " mute=" << input_mute_on_;
  SetInputMuteInternal(input_mute_on_);
  // TODO(rkc,jennyz): Set input gain once we decide on how to store
  // the gain values since the range and step are both device specific.
}

void CrasAudioHandler::SetupAudioOutputState() {
  const AudioDevice* device = GetDeviceFromId(active_output_node_id_);
  if (!device) {
    LOG_IF(ERROR, log_errors_)
        << "Can't set up audio state for unknown output device id ="
        << "0x" << std::hex << active_output_node_id_;
    return;
  }
  DCHECK(!device->is_input);
  output_mute_on_ = audio_pref_handler_->GetMuteValue(*device);
  output_volume_ = audio_pref_handler_->GetOutputVolumeValue(device);

  SetOutputMuteInternal(output_mute_on_);
  SetOutputNodeVolume(active_output_node_id_, output_volume_);
}

// This sets up the state of an additional active node.
void CrasAudioHandler::SetupAdditionalActiveAudioNodeState(uint64 node_id) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device) {
    VLOG(1) << "Can't set up audio state for unknown device id ="
            << "0x" << std::hex << node_id;
    return;
  }

  DCHECK(node_id != active_output_node_id_ && node_id != active_input_node_id_);

  // Note: The mute state is a system wide state, we don't set mute per device,
  // but just keep the mute state consistent for the active node in prefs.
  // The output volume should be set to the same value for all active output
  // devices. For input devices, we don't restore their gain value so far.
  // TODO(jennyz): crbug.com/417418, track the status for the decison if
  // we should persist input gain value in prefs.
  if (!device->is_input) {
    audio_pref_handler_->SetMuteValue(*device, IsOutputMuted());
    SetOutputNodeVolumePercent(node_id, GetOutputVolumePercent());
  }
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
    VLOG(1) << "Audio input allowed by policy, sets input id="
            << "0x" << std::hex << active_input_node_id_ << " mute=false";
    SetInputMuteInternal(false);
  } else {
    VLOG(0) << "Audio input NOT allowed by policy, sets input id="
            << "0x" << std::hex << active_input_node_id_ << " mute=true";
    SetInputMuteInternal(true);
    input_mute_locked_ = true;
  }
}

void CrasAudioHandler::SetOutputNodeVolume(uint64 node_id, int volume) {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetOutputNodeVolume(node_id, volume);
}

void CrasAudioHandler::SetOutputNodeVolumePercent(uint64 node_id,
                                                  int volume_percent) {
  const AudioDevice* device = this->GetDeviceFromId(node_id);
  if (!device || device->is_input)
    return;

  volume_percent = min(max(volume_percent, 0), 100);
  if (volume_percent <= kMuteThresholdPercent)
    volume_percent = 0;
  if (node_id == active_output_node_id_)
    output_volume_ = volume_percent;

  audio_pref_handler_->SetVolumeGainValue(*device, volume_percent);

  if (device->active) {
    SetOutputNodeVolume(node_id, volume_percent);
    FOR_EACH_OBSERVER(AudioObserver, observers_, OnOutputVolumeChanged());
  }
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

void CrasAudioHandler::SetInputNodeGainPercent(uint64 node_id,
                                               int gain_percent) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device || !device->is_input)
    return;

  // NOTE: We do not sanitize input gain values since the range is completely
  // dependent on the device.
  if (active_input_node_id_ == node_id)
    input_gain_ = gain_percent;

  audio_pref_handler_->SetVolumeGainValue(*device, gain_percent);

  if (device->active) {
    SetInputNodeGain(node_id, gain_percent);
    FOR_EACH_OBSERVER(AudioObserver, observers_, OnInputGainChanged());
  }
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
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&CrasAudioHandler::HandleGetNodesError,
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

  // Reset all other input or output devices' active status. The active audio
  // device from the previous user session can be remembered by cras, but not
  // in chrome. see crbug.com/273271.
  for (AudioDeviceMap::iterator it = audio_devices_.begin();
       it != audio_devices_.end(); ++it) {
    if (it->second.is_input == new_active_device.is_input &&
        it->second.id != new_active_device.id)
      it->second.active = false;
  }

  // Set the current active input/output device to the new_active_device.
  *current_active_node_id = new_active_device.id;
  audio_devices_[*current_active_node_id].active = true;
  return true;
}

bool CrasAudioHandler::NonActiveDeviceUnplugged(
    size_t old_devices_size,
    size_t new_devices_size,
    uint64 current_active_node) {
  return (new_devices_size < old_devices_size &&
          GetDeviceFromId(current_active_node));
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

bool CrasAudioHandler::HasDeviceChange(const AudioNodeList& new_nodes,
                                       bool is_input) {
  size_t num_old_devices = 0;
  size_t num_new_devices = 0;
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end(); ++it) {
    if (is_input == it->second.is_input)
      ++num_old_devices;
  }

  for (AudioNodeList::const_iterator it = new_nodes.begin();
       it != new_nodes.end(); ++it) {
    if (is_input == it->is_input) {
      ++num_new_devices;
      // Look to see if the new device not in the old device list.
      AudioDevice device(*it);
      if (FoundNewOrChangedDevice(device))
        return true;
    }
  }
  return num_old_devices != num_new_devices;
}

bool CrasAudioHandler::FoundNewOrChangedDevice(const AudioDevice& device) {
  const AudioDevice* device_found = GetDeviceFromId(device.id);
  if (!device_found)
    return true;

  if (!IsSameAudioDevice(device, *device_found)) {
    LOG(WARNING) << "Different Audio devices with same id:"
        << " new device: " << device.ToString()
        << " old device: " << device_found->ToString();
    return true;
  } else if (device.active != device_found->active) {
    return true;
  }

  return false;
}

void CrasAudioHandler::UpdateDevicesAndSwitchActive(
    const AudioNodeList& nodes) {
  size_t old_audio_devices_size = audio_devices_.size();
  bool output_devices_changed = HasDeviceChange(nodes, false);
  bool input_devices_changed = HasDeviceChange(nodes, true);
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
        device.type != AUDIO_TYPE_INTERNAL_MIC &&
        device.type != AUDIO_TYPE_KEYBOARD_MIC) {
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

  // If audio nodes change is caused by unplugging some non-active audio
  // devices, the previously set active audio device will stay active.
  // Otherwise, switch to a new active audio device according to their priority.
  if (input_devices_changed &&
      !NonActiveDeviceUnplugged(old_audio_devices_size,
                                audio_devices_.size(),
                                active_input_node_id_) &&
      !input_devices_pq_.empty())
    SwitchToDevice(input_devices_pq_.top());
  if (output_devices_changed &&
      !NonActiveDeviceUnplugged(old_audio_devices_size,
                                audio_devices_.size(),
                                active_output_node_id_) &&
      !output_devices_pq_.empty()) {
    SwitchToDevice(output_devices_pq_.top());
  }
}

void CrasAudioHandler::HandleGetNodes(const chromeos::AudioNodeList& node_list,
                                      bool success) {
  if (!success) {
    LOG_IF(ERROR, log_errors_) << "Failed to retrieve audio nodes data";
    return;
  }

  UpdateDevicesAndSwitchActive(node_list);
  FOR_EACH_OBSERVER(AudioObserver, observers_, OnAudioNodesChanged());
}

void CrasAudioHandler::HandleGetNodesError(const std::string& error_name,
                                           const std::string& error_msg) {
  LOG_IF(ERROR, log_errors_) << "Failed to call GetNodes: "
      << error_name  << ": " << error_msg;
}

void CrasAudioHandler::AddAdditionalActiveNode(uint64 node_id) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device) {
    VLOG(1) << "AddActiveInputNode: Cannot find device id="
            << "0x" << std::hex << node_id;
    return;
  }

  audio_devices_[node_id].active = true;
  SetupAdditionalActiveAudioNodeState(node_id);

  if (device->is_input) {
    DCHECK(node_id != active_input_node_id_);
    chromeos::DBusThreadManager::Get()
        ->GetCrasAudioClient()
        ->AddActiveInputNode(node_id);
    FOR_EACH_OBSERVER(AudioObserver, observers_, OnActiveInputNodeChanged());
  } else {
    DCHECK(node_id != active_output_node_id_);
    chromeos::DBusThreadManager::Get()
        ->GetCrasAudioClient()
        ->AddActiveOutputNode(node_id);
    FOR_EACH_OBSERVER(AudioObserver, observers_, OnActiveOutputNodeChanged());
  }
}

void CrasAudioHandler::RemoveActiveNodeInternal(uint64 node_id) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device) {
    VLOG(1) << "RemoveActiveInputNode: Cannot find device id="
            << "0x" << std::hex << node_id;
    return;
  }

  audio_devices_[node_id].active = false;
  if (device->is_input) {
    if (node_id == active_input_node_id_)
      active_input_node_id_ = 0;
    chromeos::DBusThreadManager::Get()
        ->GetCrasAudioClient()
        ->RemoveActiveInputNode(node_id);
    FOR_EACH_OBSERVER(AudioObserver, observers_, OnActiveInputNodeChanged());
  } else {
    if (node_id == active_output_node_id_)
      active_output_node_id_ = 0;
    chromeos::DBusThreadManager::Get()
        ->GetCrasAudioClient()
        ->RemoveActiveOutputNode(node_id);
  }
  FOR_EACH_OBSERVER(AudioObserver, observers_, OnActiveOutputNodeChanged());
}

}  // namespace chromeos
