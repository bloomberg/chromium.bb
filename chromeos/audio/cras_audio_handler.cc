// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/cras_audio_handler.h"

#include <stddef.h>
#include <stdint.h>

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

// The duration of HDMI output re-discover grace period in milliseconds.
const int kHDMIRediscoverGracePeriodDurationInMs = 2000;

static CrasAudioHandler* g_cras_audio_handler = NULL;

bool IsSameAudioDevice(const AudioDevice& a, const AudioDevice& b) {
  return a.id == b.id && a.is_input == b.is_input && a.type == b.type
      && a.device_name == b.device_name;
}

bool IsInNodeList(uint64_t node_id,
                  const CrasAudioHandler::NodeIdList& id_list) {
  return std::find(id_list.begin(), id_list.end(), node_id) != id_list.end();
}

bool IsNodeInTheList(uint64_t node_id, const AudioNodeList& node_list) {
  for (size_t i = 0; i < node_list.size(); ++i) {
    if (node_id == node_list[i].id)
      return true;
  }
  return false;
}

}  // namespace

CrasAudioHandler::AudioObserver::AudioObserver() {
}

CrasAudioHandler::AudioObserver::~AudioObserver() {
}

void CrasAudioHandler::AudioObserver::OnOutputNodeVolumeChanged(
    uint64_t /* node_id */,
    int /* volume */) {
}

void CrasAudioHandler::AudioObserver::OnInputNodeGainChanged(
    uint64_t /* node_id */,
    int /* gain */) {
}

void CrasAudioHandler::AudioObserver::OnOutputMuteChanged(
    bool /* mute_on */,
    bool /* system_adjust */) {}

void CrasAudioHandler::AudioObserver::OnInputMuteChanged(bool /* mute_on */) {
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

bool CrasAudioHandler::IsOutputMutedForDevice(uint64_t device_id) {
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

bool CrasAudioHandler::IsInputMutedForDevice(uint64_t device_id) {
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

int CrasAudioHandler::GetOutputVolumePercentForDevice(uint64_t device_id) {
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

int CrasAudioHandler::GetInputGainPercentForDevice(uint64_t device_id) {
  if (device_id == active_input_node_id_) {
    return input_gain_;
  } else {
    const AudioDevice* device = GetDeviceFromId(device_id);
    return static_cast<int>(audio_pref_handler_->GetInputGainValue(device));
  }
}

uint64_t CrasAudioHandler::GetPrimaryActiveOutputNode() const {
  return active_output_node_id_;
}

uint64_t CrasAudioHandler::GetPrimaryActiveInputNode() const {
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
    AddActiveNode(keyboard_mic->id, true);
  else
    RemoveActiveNodeInternal(keyboard_mic->id, true);
}

void CrasAudioHandler::AddActiveNode(uint64_t node_id, bool notify) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device) {
    VLOG(1) << "AddActiveInputNode: Cannot find device id="
            << "0x" << std::hex << node_id;
    return;
  }

  // If there is no primary active device, set |node_id| to primary active node.
  if ((device->is_input && !active_input_node_id_) ||
      (!device->is_input && !active_output_node_id_)) {
    SwitchToDevice(*device, notify);
    return;
  }

  AddAdditionalActiveNode(node_id, notify);
}

void CrasAudioHandler::ChangeActiveNodes(const NodeIdList& new_active_ids) {
  // Flags for whether there are input or output nodes passed in from
  // |new_active_ids|. If there are no input nodes passed in, we will not
  // make any change for input nodes; same for the output nodes.
  bool request_input_change = false;
  bool request_output_change = false;

  // Flags for whether we will actually change active status of input
  // or output nodes.
  bool make_input_change = false;
  bool make_output_change = false;

  NodeIdList nodes_to_activate;
  for (size_t i = 0; i < new_active_ids.size(); ++i) {
    const AudioDevice* device = GetDeviceFromId(new_active_ids[i]);
    if (device) {
      if (device->is_input)
        request_input_change = true;
      else
        request_output_change = true;

      // If the new active device is already active, keep it as active.
      if (device->active)
        continue;

      nodes_to_activate.push_back(new_active_ids[i]);
      if (device->is_input)
        make_input_change = true;
      else
        make_output_change = true;
    }
  }

  // Remove all existing active devices that are not in the |new_active_ids|
  // list.
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end(); ++it) {
    AudioDevice device = it->second;
    // Remove the existing active input or output nodes that are not in the new
    // active node list if there are new input or output nodes specified.
    if (device.active) {
      if ((device.is_input && request_input_change &&
           !IsInNodeList(device.id, new_active_ids))) {
        make_input_change = true;
        RemoveActiveNodeInternal(device.id, false);  // no notification.
      } else if (!device.is_input && request_output_change &&
                 !IsInNodeList(device.id, new_active_ids)) {
        make_output_change = true;
        RemoveActiveNodeInternal(device.id, false);  // no notification.
      }
    }
  }

  // Adds the new active devices.
  for (size_t i = 0; i < nodes_to_activate.size(); ++i)
    AddActiveNode(nodes_to_activate[i], false);  // no notification.

  // Notify the active nodes change now.
  if (make_input_change)
    NotifyActiveNodeChanged(true);
  if (make_output_change)
    NotifyActiveNodeChanged(false);
}

void CrasAudioHandler::SwapInternalSpeakerLeftRightChannel(bool swap) {
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end();
       ++it) {
    const AudioDevice& device = it->second;
    if (!device.is_input && device.type == AUDIO_TYPE_INTERNAL_SPEAKER) {
      chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->SwapLeftRight(
          device.id, swap);
      break;
    }
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

  FOR_EACH_OBSERVER(
      AudioObserver, observers_,
      OnOutputMuteChanged(output_mute_on_, false /* system_adjust */));
}

void CrasAudioHandler::AdjustOutputVolumeToAudibleLevel() {
  if (output_volume_ <= kMuteThresholdPercent) {
    // Avoid the situation when sound has been unmuted, but the volume
    // is set to a very low value, so user still can't hear any sound.
    SetOutputVolumePercent(kDefaultUnmuteVolumePercent);
  }
}

void CrasAudioHandler::SetInputMute(bool mute_on) {
  SetInputMuteInternal(mute_on);
  FOR_EACH_OBSERVER(AudioObserver, observers_,
                    OnInputMuteChanged(input_mute_on_));
}

void CrasAudioHandler::SetActiveOutputNode(uint64_t node_id, bool notify) {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetActiveOutputNode(node_id);
  if (notify)
    NotifyActiveNodeChanged(false);
}

void CrasAudioHandler::SetActiveInputNode(uint64_t node_id, bool notify) {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetActiveInputNode(node_id);
  if (notify)
    NotifyActiveNodeChanged(true);
}

void CrasAudioHandler::SetVolumeGainPercentForDevice(uint64_t device_id,
                                                     int value) {
  const AudioDevice* device = GetDeviceFromId(device_id);
  if (!device)
    return;

  if (device->is_input)
    SetInputNodeGainPercent(device_id, value);
  else
    SetOutputNodeVolumePercent(device_id, value);
}

void CrasAudioHandler::SetMuteForDevice(uint64_t device_id, bool mute_on) {
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

// If the HDMI device is the active output device, when the device enters/exits
// docking mode, or HDMI display changes resolution, or chromeos device
// suspends/resumes, cras will lose the HDMI output node for a short period of
// time, then rediscover it. This hotplug behavior will cause the audio output
// be leaked to the alternatvie active audio output during HDMI re-discovering
// period. See crbug.com/503667.
void CrasAudioHandler::SetActiveHDMIOutoutRediscoveringIfNecessary(
    bool force_rediscovering) {
  if (!GetDeviceFromId(active_output_node_id_))
    return;

  // Marks the start of the HDMI re-discovering grace period, during which we
  // will mute the audio output to prevent it to be be leaked to the
  // alternative output device.
  if ((hdmi_rediscovering_ && force_rediscovering) ||
      (!hdmi_rediscovering_ && IsHDMIPrimaryOutputDevice())) {
    StartHDMIRediscoverGracePeriod();
  }
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
      log_errors_(false),
      hdmi_rediscover_grace_period_duration_in_ms_(
          kHDMIRediscoverGracePeriodDurationInMs),
      hdmi_rediscovering_(false),
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
  hdmi_rediscover_timer_.Stop();
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

void CrasAudioHandler::ActiveOutputNodeChanged(uint64_t node_id) {
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

void CrasAudioHandler::ActiveInputNodeChanged(uint64_t node_id) {
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

const AudioDevice* CrasAudioHandler::GetDeviceFromId(uint64_t device_id) const {
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
  // Mute the output during HDMI re-discovering grace period.
  if (hdmi_rediscovering_ && !IsHDMIPrimaryOutputDevice()) {
    VLOG(1) << "Mute the output during HDMI re-discovering grace period";
    output_mute_on_ = true;
  } else {
    output_mute_on_ = audio_pref_handler_->GetMuteValue(*device);
  }
  output_volume_ = audio_pref_handler_->GetOutputVolumeValue(device);

  SetOutputMuteInternal(output_mute_on_);
  SetOutputNodeVolume(active_output_node_id_, output_volume_);
}

// This sets up the state of an additional active node.
void CrasAudioHandler::SetupAdditionalActiveAudioNodeState(uint64_t node_id) {
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

  // Policy for audio input is handled by kAudioCaptureAllowed in the Chrome
  // media system.
}

void CrasAudioHandler::SetOutputNodeVolume(uint64_t node_id, int volume) {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetOutputNodeVolume(node_id, volume);
}

void CrasAudioHandler::SetOutputNodeVolumePercent(uint64_t node_id,
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
    FOR_EACH_OBSERVER(AudioObserver, observers_,
                      OnOutputNodeVolumeChanged(node_id, volume_percent));
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

void CrasAudioHandler::SetInputNodeGain(uint64_t node_id, int gain) {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetInputNodeGain(node_id, gain);
}

void CrasAudioHandler::SetInputNodeGainPercent(uint64_t node_id,
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
    FOR_EACH_OBSERVER(AudioObserver, observers_,
                      OnInputNodeGainChanged(node_id, gain_percent));
  }
}

void CrasAudioHandler::SetInputMuteInternal(bool mute_on) {
  input_mute_on_ = mute_on;
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      SetInputMute(mute_on);
}

void CrasAudioHandler::GetNodes() {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->GetNodes(
      base::Bind(&CrasAudioHandler::HandleGetNodes,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&CrasAudioHandler::HandleGetNodesError,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool CrasAudioHandler::ChangeActiveDevice(const AudioDevice& new_active_device,
                                          uint64_t* current_active_node_id) {
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

bool CrasAudioHandler::NonActiveDeviceUnplugged(size_t old_devices_size,
                                                size_t new_devices_size,
                                                uint64_t current_active_node) {
  return (new_devices_size < old_devices_size &&
          GetDeviceFromId(current_active_node));
}

void CrasAudioHandler::SwitchToDevice(const AudioDevice& device, bool notify) {
  if (device.is_input) {
    if (!ChangeActiveDevice(device, &active_input_node_id_))
      return;
    SetupAudioInputState();
    SetActiveInputNode(active_input_node_id_, notify);
  } else {
    if (!ChangeActiveDevice(device, &active_output_node_id_))
      return;
    SetupAudioOutputState();
    SetActiveOutputNode(active_output_node_id_, notify);
  }
}

bool CrasAudioHandler::HasDeviceChange(const AudioNodeList& new_nodes,
                                       bool is_input,
                                       AudioNodeList* new_discovered) {
  size_t num_old_devices = 0;
  size_t num_new_devices = 0;
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end(); ++it) {
    if (is_input == it->second.is_input)
      ++num_old_devices;
  }

  bool new_or_changed_device = false;
  new_discovered->clear();
  for (AudioNodeList::const_iterator it = new_nodes.begin();
       it != new_nodes.end(); ++it) {
    if (is_input == it->is_input) {
      ++num_new_devices;
      // Look to see if the new device not in the old device list.
      AudioDevice device(*it);
      DeviceStatus status = CheckDeviceStatus(device);
      if (status == NEW_DEVICE)
        new_discovered->push_back(*it);
      if (status == NEW_DEVICE || status == CHANGED_DEVICE) {
        new_or_changed_device = true;
      }
    }
  }
  return new_or_changed_device || (num_old_devices != num_new_devices);
}

CrasAudioHandler::DeviceStatus CrasAudioHandler::CheckDeviceStatus(
    const AudioDevice& device) {
  const AudioDevice* device_found = GetDeviceFromId(device.id);
  if (!device_found)
    return NEW_DEVICE;

  if (!IsSameAudioDevice(device, *device_found)) {
    LOG(WARNING) << "Different Audio devices with same id:"
        << " new device: " << device.ToString()
        << " old device: " << device_found->ToString();
    return CHANGED_DEVICE;
  } else if (device.active != device_found->active) {
    return CHANGED_DEVICE;
  }

  return OLD_DEVICE;
}

void CrasAudioHandler::NotifyActiveNodeChanged(bool is_input) {
  if (is_input)
    FOR_EACH_OBSERVER(AudioObserver, observers_, OnActiveInputNodeChanged());
  else
    FOR_EACH_OBSERVER(AudioObserver, observers_, OnActiveOutputNodeChanged());
}

void CrasAudioHandler::UpdateDevicesAndSwitchActive(
    const AudioNodeList& nodes) {
  size_t old_output_device_size = 0;
  size_t old_input_device_size = 0;
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end(); ++it) {
    if (it->second.is_input)
      ++old_input_device_size;
    else
      ++old_output_device_size;
  }

  AudioNodeList hotplug_output_nodes;
  AudioNodeList hotplug_input_nodes;
  bool output_devices_changed =
      HasDeviceChange(nodes, false, &hotplug_output_nodes);
  bool input_devices_changed =
      HasDeviceChange(nodes, true, &hotplug_input_nodes);
  audio_devices_.clear();
  has_alternative_input_ = false;
  has_alternative_output_ = false;

  while (!input_devices_pq_.empty())
    input_devices_pq_.pop();
  while (!output_devices_pq_.empty())
    output_devices_pq_.pop();

  size_t new_output_device_size = 0;
  size_t new_input_device_size = 0;
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

    if (device.is_input) {
      input_devices_pq_.push(device);
      ++new_input_device_size;
    } else {
      output_devices_pq_.push(device);
      ++new_output_device_size;
    }
  }

  // If the previous active device is removed from the new node list,
  // or changed to inactive by cras, reset active_output_node_id_.
  // See crbug.com/478968.
  const AudioDevice* active_output = GetDeviceFromId(active_output_node_id_);
  if (!active_output || !active_output->active)
    active_output_node_id_ = 0;
  const AudioDevice* active_input = GetDeviceFromId(active_input_node_id_);
  if (!active_input || !active_input->active)
    active_input_node_id_ = 0;

  // If audio nodes change is caused by unplugging some non-active audio
  // devices, the previously set active audio device will stay active.
  // Otherwise, switch to a new active audio device according to their priority.
  if (input_devices_changed &&
      !NonActiveDeviceUnplugged(old_input_device_size,
                                new_input_device_size,
                                active_input_node_id_)) {
    // Some devices like chromeboxes don't have the internal audio input. In
    // that case the active input node id should be reset.
    if (input_devices_pq_.empty()) {
      active_input_node_id_ = 0;
      NotifyActiveNodeChanged(true);
    } else {
      // If user has hot plugged a new node, we should change to the active
      // device to the new node if it has the highest priority; otherwise,
      // we should keep the existing active node chosen by user.
      // For all other cases, we will choose the node with highest priority.
      if (!active_input_node_id_ || hotplug_input_nodes.empty() ||
          IsNodeInTheList(input_devices_pq_.top().id, hotplug_input_nodes)) {
        SwitchToDevice(input_devices_pq_.top(), true);
      }
    }
  }
  if (output_devices_changed &&
      !NonActiveDeviceUnplugged(old_output_device_size,
                                new_output_device_size,
                                active_output_node_id_)) {
    // This is really unlikely to happen because all ChromeOS devices have the
    // internal audio output.
    if (output_devices_pq_.empty()) {
      active_output_node_id_ = 0;
      NotifyActiveNodeChanged(false);
    } else {
      // ditto input node case.
      if (!active_output_node_id_ || hotplug_output_nodes.empty() ||
          IsNodeInTheList(output_devices_pq_.top().id, hotplug_output_nodes)) {
        SwitchToDevice(output_devices_pq_.top(), true);
      }
    }
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

void CrasAudioHandler::AddAdditionalActiveNode(uint64_t node_id, bool notify) {
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
    if (notify)
      NotifyActiveNodeChanged(true);
  } else {
    DCHECK(node_id != active_output_node_id_);
    chromeos::DBusThreadManager::Get()
        ->GetCrasAudioClient()
        ->AddActiveOutputNode(node_id);
    if (notify)
      NotifyActiveNodeChanged(false);
  }
}

void CrasAudioHandler::RemoveActiveNodeInternal(uint64_t node_id, bool notify) {
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
    if (notify)
      NotifyActiveNodeChanged(true);
  } else {
    if (node_id == active_output_node_id_)
      active_output_node_id_ = 0;
    chromeos::DBusThreadManager::Get()
        ->GetCrasAudioClient()
        ->RemoveActiveOutputNode(node_id);
    if (notify)
      NotifyActiveNodeChanged(false);
  }
}

void CrasAudioHandler::UpdateAudioAfterHDMIRediscoverGracePeriod() {
  VLOG(1) << "HDMI output re-discover grace period ends.";
  hdmi_rediscovering_ = false;
  if (!IsOutputMutedForDevice(active_output_node_id_)) {
    // Unmute the audio output after the HDMI transition period.
    VLOG(1) << "Unmute output after HDMI rediscovering grace period.";
    SetOutputMuteInternal(false);

    // Notify UI about the mute state change.
    FOR_EACH_OBSERVER(
        AudioObserver, observers_,
        OnOutputMuteChanged(output_mute_on_, true /* system adjustment */));
  }
}

bool CrasAudioHandler::IsHDMIPrimaryOutputDevice() const {
  const AudioDevice* device = GetDeviceFromId(active_output_node_id_);
  return (device && device->type == chromeos::AUDIO_TYPE_HDMI);
}

void CrasAudioHandler::StartHDMIRediscoverGracePeriod() {
  VLOG(1) << "Start HDMI rediscovering grace period.";
  hdmi_rediscovering_ = true;
  hdmi_rediscover_timer_.Stop();
  hdmi_rediscover_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(
                     hdmi_rediscover_grace_period_duration_in_ms_),
      this, &CrasAudioHandler::UpdateAudioAfterHDMIRediscoverGracePeriod);
}

void CrasAudioHandler::SetHDMIRediscoverGracePeriodForTesting(
    int duration_in_ms) {
  hdmi_rediscover_grace_period_duration_in_ms_ = duration_in_ms;
}

}  // namespace chromeos
