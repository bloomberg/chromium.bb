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
#include "base/sys_info.h"
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

// Mixer matrix, [0.5, 0.5; 0.5, 0.5]
const std::vector<double> kStereoToMono = {0.5, 0.5, 0.5, 0.5};
// Mixer matrix, [1, 0; 0, 1]
const std::vector<double> kStereoToStereo = {1, 0, 0, 1};

static CrasAudioHandler* g_cras_audio_handler = NULL;

bool IsSameAudioDevice(const AudioDevice& a, const AudioDevice& b) {
  return a.stable_device_id == b.stable_device_id && a.is_input == b.is_input &&
         a.type == b.type && a.device_name == b.device_name;
}

bool IsInNodeList(uint64_t node_id,
                  const CrasAudioHandler::NodeIdList& id_list) {
  return std::find(id_list.begin(), id_list.end(), node_id) != id_list.end();
}

bool IsDeviceInList(const AudioDevice& device, const AudioNodeList& node_list) {
  for (const AudioNode& node : node_list) {
    if (device.stable_device_id == node.stable_device_id)
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

void CrasAudioHandler::AudioObserver::OnOuputChannelRemixingChanged(
    bool /* mono_on */) {
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
    SwitchToDevice(*device, notify, ACTIVATE_BY_USER);
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

  // Adds the new active devices. Note that this function is used by audio
  // extension to manage multiple active nodes, in which case the devices
  // selection preference is controlled by the app, we intentionally exclude
  // the additional nodes from saving their device states as a result.
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

void CrasAudioHandler::SetOutputMono(bool mono_on) {
  output_mono_on_ = mono_on;
  if (mono_on) {
    chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
        SetGlobalOutputChannelRemix(output_channels_, kStereoToMono);
  } else {
    chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
        SetGlobalOutputChannelRemix(output_channels_, kStereoToStereo);
  }

  FOR_EACH_OBSERVER(
      AudioObserver, observers_,
      OnOuputChannelRemixingChanged(mono_on));
}

bool CrasAudioHandler::IsOutputMonoEnabled() const {
  return output_mono_on_;
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

void CrasAudioHandler::SetActiveDevice(const AudioDevice& active_device,
                                       bool notify,
                                       DeviceActivateType activate_by) {
  if (active_device.is_input) {
    chromeos::DBusThreadManager::Get()
        ->GetCrasAudioClient()
        ->SetActiveInputNode(active_device.id);
  } else {
    chromeos::DBusThreadManager::Get()
        ->GetCrasAudioClient()
        ->SetActiveOutputNode(active_device.id);
  }

  if (notify)
    NotifyActiveNodeChanged(active_device.is_input);

  // Save active state for the nodes.
  for (AudioDeviceMap::iterator it = audio_devices_.begin();
       it != audio_devices_.end(); ++it) {
    const AudioDevice& device = it->second;
    if (device.is_input != active_device.is_input)
      continue;
    SaveDeviceState(device, device.active, activate_by);
  }
}

void CrasAudioHandler::SaveDeviceState(const AudioDevice& device,
                                       bool active,
                                       DeviceActivateType activate_by) {
  // Don't save the active state for non-simple usage device, which is invisible
  // to end users.
  if (!device.is_for_simple_usage())
    return;

  if (!active) {
    audio_pref_handler_->SetDeviceActive(device, false, false);
  } else {
    switch (activate_by) {
      case ACTIVATE_BY_USER:
        audio_pref_handler_->SetDeviceActive(device, true, true);
        break;
      case ACTIVATE_BY_PRIORITY:
        audio_pref_handler_->SetDeviceActive(device, true, false);
        break;
      default:
        // The device is made active due to its previous active state in prefs,
        // don't change its active state settings in prefs.
        break;
    }
  }
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
      output_channels_(2),
      output_mono_on_(false),
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
  if (cras_service_available_)
    GetNodes();
}

void CrasAudioHandler::OutputNodeVolumeChanged(uint64_t node_id, int volume) {
  const AudioDevice* device = this->GetDeviceFromId(node_id);

  // If this is not an active output node, ignore this event. Because when this
  // node set to active, it will be applied with the volume value stored in
  // preference.
  if (!device || !device->active || device->is_input) {
    LOG(ERROR) << "Unexpexted OutputNodeVolumeChanged received on node: 0x"
               << std::hex << node_id;
    return;
  }

  // Sync internal volume state and notify UI for the change. We trust cras
  // signal to report the volume state of the device, no matter which source
  // set the volume, i.e., volume could be set from non-chrome source, like
  // Bluetooth headset, etc. Assume all active output devices share a single
  // volume.
  output_volume_ = volume;
  audio_pref_handler_->SetVolumeGainValue(*device, volume);

  if (initializing_audio_state_) {
    // Reset the flag after the first OutputNodeVolumeChanged, just in case
    // cras didn't respond to the initial SetOutputNodeVolume request.
    initializing_audio_state_ = false;
    // Do not notify the observers for volume changed event if CrasAudioHandler
    // is initializing its state, i.e., the volume change event is not from
    // user action, no need to notify UI to pop uo the volume slider bar.
    if (init_node_id_ == node_id && init_volume_ ==  volume)
      return;
  }

  FOR_EACH_OBSERVER(AudioObserver, observers_,
                    OnOutputNodeVolumeChanged(node_id, volume));
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

const AudioDevice* CrasAudioHandler::GetDeviceFromStableDeviceId(
    uint64_t stable_device_id) const {
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end(); ++it) {
    if (it->second.stable_device_id == stable_device_id)
      return &(it->second);
  }
  return NULL;
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

  if (initializing_audio_state_) {
    init_node_id_ = active_output_node_id_;
    init_volume_ = output_volume_;
  }
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
  initializing_audio_state_ = true;
  ApplyAudioPolicy();

  // Defer querying cras for GetNodes until cras service becomes available.
  cras_service_available_ = false;
  chromeos::DBusThreadManager::Get()
      ->GetCrasAudioClient()
      ->WaitForServiceToBeAvailable(base::Bind(
          &CrasAudioHandler::InitializeAudioAfterCrasServiceAvailable,
          weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::InitializeAudioAfterCrasServiceAvailable(
    bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "Cras service is not available";
    cras_service_available_ = false;
    return;
  }

  cras_service_available_ = true;
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
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device || device->is_input)
    return;

  volume_percent = min(max(volume_percent, 0), 100);
  if (volume_percent <= kMuteThresholdPercent)
    volume_percent = 0;

  // Save the volume setting in pref in case this is called on non-active
  // node for configuration.
  audio_pref_handler_->SetVolumeGainValue(*device, volume_percent);

  if (device->active)
    SetOutputNodeVolume(node_id, volume_percent);
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

bool CrasAudioHandler::ChangeActiveDevice(
    const AudioDevice& new_active_device) {
  uint64_t& current_active_node_id = new_active_device.is_input
                                         ? active_input_node_id_
                                         : active_output_node_id_;
  // If the device we want to switch to is already the current active device,
  // do nothing.
  if (new_active_device.active &&
      new_active_device.id == current_active_node_id) {
    return false;
  }

  bool found_new_active_device = false;
  // Reset all other input or output devices' active status. The active audio
  // device from the previous user session can be remembered by cras, but not
  // in chrome. see crbug.com/273271.
  for (AudioDeviceMap::iterator it = audio_devices_.begin();
       it != audio_devices_.end(); ++it) {
    if (it->second.is_input == new_active_device.is_input &&
        it->second.id != new_active_device.id) {
      it->second.active = false;
    } else if (it->second.is_input == new_active_device.is_input &&
               it->second.id == new_active_device.id) {
      found_new_active_device = true;
    }
  }

  if (!found_new_active_device) {
    LOG(ERROR) << "Invalid new active device: " << new_active_device.ToString();
    return false;
  }

  // Set the current active input/output device to the new_active_device.
  current_active_node_id = new_active_device.id;
  audio_devices_[current_active_node_id].active = true;
  return true;
}

void CrasAudioHandler::SwitchToDevice(const AudioDevice& device,
                                      bool notify,
                                      DeviceActivateType activate_by) {
  if (!ChangeActiveDevice(device))
    return;

  if (device.is_input)
    SetupAudioInputState();
  else
    SetupAudioOutputState();

  SetActiveDevice(device, notify, activate_by);
}

bool CrasAudioHandler::HasDeviceChange(const AudioNodeList& new_nodes,
                                       bool is_input,
                                       AudioDevicePriorityQueue* new_discovered,
                                       bool* device_removed,
                                       bool* active_device_removed) {
  *device_removed = false;
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end(); ++it) {
    const AudioDevice& device = it->second;
    if (is_input != device.is_input)
      continue;
    if (!IsDeviceInList(device, new_nodes)) {
      *device_removed = true;
      if ((is_input && device.id == active_input_node_id_) ||
          (!is_input && device.id == active_output_node_id_)) {
        *active_device_removed = true;
      }
    }
  }

  bool new_or_changed_device = false;
  while (!new_discovered->empty())
    new_discovered->pop();
  for (AudioNodeList::const_iterator it = new_nodes.begin();
       it != new_nodes.end(); ++it) {
    if (is_input != it->is_input)
      continue;
    // Check if the new device is not in the old device list.
    AudioDevice device(*it);
    DeviceStatus status = CheckDeviceStatus(device);
    if (status == NEW_DEVICE)
      new_discovered->push(device);
    if (status == NEW_DEVICE || status == CHANGED_DEVICE) {
      new_or_changed_device = true;
    }
  }
  return new_or_changed_device || *device_removed;
}

CrasAudioHandler::DeviceStatus CrasAudioHandler::CheckDeviceStatus(
    const AudioDevice& device) {
  const AudioDevice* device_found =
      GetDeviceFromStableDeviceId(device.stable_device_id);
  if (!device_found)
    return NEW_DEVICE;

  if (!IsSameAudioDevice(device, *device_found)) {
    LOG(ERROR) << "Different Audio devices with same stable device id:"
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

bool CrasAudioHandler::GetActiveDeviceFromUserPref(bool is_input,
                                                   AudioDevice* active_device) {
  bool found_active_device = false;
  bool last_active_device_activate_by_user = false;
  for (AudioDeviceMap::const_iterator it = audio_devices_.begin();
       it != audio_devices_.end(); ++it) {
    AudioDevice device = it->second;
    if (device.is_input != is_input || !device.is_for_simple_usage())
      continue;

    bool active = false;
    bool activate_by_user = false;
    if (!audio_pref_handler_->GetDeviceActive(device, &active,
                                              &activate_by_user) ||
        !active) {
      continue;
    }

    if (!found_active_device) {
      found_active_device = true;
      *active_device = device;
      last_active_device_activate_by_user = activate_by_user;
      continue;
    }

    // Choose the best one among multiple active devices from prefs.
    if (activate_by_user) {
      if (!last_active_device_activate_by_user) {
        // Device activated by user has higher priority than the one
        // is not activated by user.
        *active_device = device;
        last_active_device_activate_by_user = true;
      } else {
        // If there are more than one active devices activated by user in the
        // prefs, most likely, after the device was shut down, and before it
        // is rebooted, user has plugged in some previously unplugged audio
        // devices. For such case, it does not make sense to honor the active
        // states in the prefs.
        VLOG(1) << "Found more than one user activated devices in the prefs.";
        return false;
      }
    } else if (!last_active_device_activate_by_user) {
      // If there are more than one active devices activated by priority in the
      // prefs, most likely, cras is still enumerating the audio devices
      // progressively. For such case, it does not make sense to honor the
      // active states in the prefs.
      VLOG(1) << "Found more than one active devices by priority in the prefs.";
      return false;
    }
  }

  if (found_active_device && !active_device->is_for_simple_usage()) {
    // This is an odd case which is rare but possible to happen during cras
    // initialization depeneding the audio device enumation process. The only
    // audio node coming from cras is an internal audio device not visible
    // to user, such as AUDIO_TYPE_POST_MIX_LOOPBACK.
    return false;
  }

  return found_active_device;
}

void CrasAudioHandler::HandleNonHotplugNodesChange(
    bool is_input,
    const AudioDevicePriorityQueue& hotplug_nodes,
    bool has_device_change,
    bool has_device_removed,
    bool active_device_removed) {
  bool has_current_active_node =
      is_input ? active_input_node_id_ : active_output_node_id_;

  // No device change, extra NodesChanged signal received.
  if (!has_device_change && has_current_active_node)
    return;

  if (hotplug_nodes.empty()) {
    if (has_device_removed) {
      if (!active_device_removed && has_current_active_node) {
        // Removed a non-active device, keep the current active device.
        return;
      }

      if (active_device_removed) {
        // Unplugged the current active device.
        SwitchToTopPriorityDevice(is_input);
        return;
      }
    }

    // Some unexpected error happens on cras side. See crbug.com/586026.
    // Either cras sent stale nodes to chrome again or cras triggered some
    // error. Restore the previously selected active.
    VLOG(1) << "Odd case from cras, the active node is lost unexpectedly.";
    SwitchToPreviousActiveDeviceIfAvailable(is_input);
  } else {
    // Looks like a new chrome session starts.
    SwitchToPreviousActiveDeviceIfAvailable(is_input);
  }
}

void CrasAudioHandler::HandleHotPlugDevice(
    const AudioDevice& hotplug_device,
    const AudioDevicePriorityQueue& device_priority_queue) {
  // This most likely may happen during the transition period of cras
  // initialization phase, in which a non-simple-usage node may appear like
  // a hotplug node.
  if (!hotplug_device.is_for_simple_usage())
    return;

  bool last_state_active = false;
  bool last_activate_by_user = false;
  if (!audio_pref_handler_->GetDeviceActive(hotplug_device, &last_state_active,
                                            &last_activate_by_user)) {
    // |hotplug_device| is plugged in for the first time, activate it if it
    // is of the highest priority.
    if (device_priority_queue.top().id == hotplug_device.id) {
      VLOG(1) << "Hotplug a device for the first time: "
              << hotplug_device.ToString();
      SwitchToDevice(hotplug_device, true, ACTIVATE_BY_PRIORITY);
    }
  } else if (last_state_active) {
    VLOG(1) << "Hotplug a device, restore to active: "
            << hotplug_device.ToString();
    SwitchToDevice(hotplug_device, true, ACTIVATE_BY_RESTORE_PREVIOUS_STATE);
  } else {
    // Do not active the device if its previous state is inactive.
    VLOG(1) << "Hotplug device remains inactive as its previous state:"
            << hotplug_device.ToString();
  }
}

void CrasAudioHandler::SwitchToTopPriorityDevice(bool is_input) {
  AudioDevice top_device =
      is_input ? input_devices_pq_.top() : output_devices_pq_.top();
  if (top_device.is_for_simple_usage())
    SwitchToDevice(top_device, true, ACTIVATE_BY_PRIORITY);
}

void CrasAudioHandler::SwitchToPreviousActiveDeviceIfAvailable(bool is_input) {
  AudioDevice previous_active_device;
  if (GetActiveDeviceFromUserPref(is_input, &previous_active_device)) {
    DCHECK(previous_active_device.is_for_simple_usage());
    // Switch to previous active device stored in user prefs.
    SwitchToDevice(previous_active_device, true,
                   ACTIVATE_BY_RESTORE_PREVIOUS_STATE);
  } else {
    // No previous active device, switch to the top priority device.
    SwitchToTopPriorityDevice(is_input);
  }
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

  AudioDevicePriorityQueue hotplug_output_nodes;
  AudioDevicePriorityQueue hotplug_input_nodes;
  bool has_output_removed = false;
  bool has_input_removed = false;
  bool active_output_removed = false;
  bool active_input_removed = false;
  bool output_devices_changed =
      HasDeviceChange(nodes, false, &hotplug_output_nodes, &has_output_removed,
                      &active_output_removed);
  bool input_devices_changed =
      HasDeviceChange(nodes, true, &hotplug_input_nodes, &has_input_removed,
                      &active_input_removed);
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

  // Handle output device changes.
  HandleAudioDeviceChange(false, output_devices_pq_, hotplug_output_nodes,
                          output_devices_changed, has_output_removed,
                          active_output_removed);

  // Handle input device changes.
  HandleAudioDeviceChange(true, input_devices_pq_, hotplug_input_nodes,
                          input_devices_changed, has_input_removed,
                          active_input_removed);
}

void CrasAudioHandler::HandleAudioDeviceChange(
    bool is_input,
    const AudioDevicePriorityQueue& devices_pq,
    const AudioDevicePriorityQueue& hotplug_nodes,
    bool has_device_change,
    bool has_device_removed,
    bool active_device_removed) {
  uint64_t& active_node_id =
      is_input ? active_input_node_id_ : active_output_node_id_;

  // No audio devices found.
  if (devices_pq.empty()) {
    VLOG(1) << "No " << (is_input ? "input" : "output") << " devices found";
    active_node_id = 0;
    NotifyActiveNodeChanged(is_input);
    return;
  }

  // If the previous active device is removed from the new node list,
  // or changed to inactive by cras, reset active_node_id.
  // See crbug.com/478968.
  const AudioDevice* active_device = GetDeviceFromId(active_node_id);
  if (!active_device || !active_device->active)
    active_node_id = 0;

  if (!active_node_id || hotplug_nodes.empty() || hotplug_nodes.size() > 1) {
    HandleNonHotplugNodesChange(is_input, hotplug_nodes, has_device_change,
                                has_device_removed, active_device_removed);
  } else {
    // Typical user hotplug case.
    HandleHotPlugDevice(hotplug_nodes.top(), devices_pq);
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
