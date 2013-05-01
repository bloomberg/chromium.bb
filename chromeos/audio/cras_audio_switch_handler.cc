// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/cras_audio_switch_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "chromeos/audio/audio_pref_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"

using std::max;
using std::min;

namespace chromeos {

namespace {

static CrasAudioSwitchHandler* g_cras_audio_handler = NULL;

}  // namespace

// static
void CrasAudioSwitchHandler::Initialize() {
  CHECK(!g_cras_audio_handler);
  g_cras_audio_handler = new CrasAudioSwitchHandler();
}

// static
void CrasAudioSwitchHandler::Shutdown() {
  CHECK(g_cras_audio_handler);
  delete g_cras_audio_handler;
  g_cras_audio_handler = NULL;
}

// static
CrasAudioSwitchHandler* CrasAudioSwitchHandler::Get() {
  CHECK(g_cras_audio_handler)
      << "CrasAudioSwitchHandler::Get() called before Initialize().";
  return g_cras_audio_handler;
}

CrasAudioSwitchHandler::CrasAudioSwitchHandler()
    : muted_device_id_(0),
      weak_ptr_factory_(this) {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->AddObserver(this);
  GetNodes();
}

CrasAudioSwitchHandler::~CrasAudioSwitchHandler() {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->
      RemoveObserver(this);
}

void CrasAudioSwitchHandler::AudioClientRestarted() {
  // Get the initial audio data.
  GetNodes();
}

void CrasAudioSwitchHandler::NodesChanged() {
  // Refresh audio nodes data.
  GetNodes();
}

void CrasAudioSwitchHandler::ActiveOutputNodeChanged(uint64 node_id) {
  if (muted_device_id_ == node_id) {
    LOG(WARNING) << "Active output device switched to: " << node_id;
    muted_device_id_ = 0;
    DBusThreadManager::Get()->GetCrasAudioClient()->SetOutputMute(false);
  }
}

void CrasAudioSwitchHandler::ActiveInputNodeChanged(uint64 node_id) {
  if (muted_device_id_ == node_id) {
    LOG(WARNING) << "Active input device switched to: " << node_id;
    muted_device_id_ = 0;
    DBusThreadManager::Get()->GetCrasAudioClient()->SetInputMute(false);
  }
}

void CrasAudioSwitchHandler::GetNodes() {
  chromeos::DBusThreadManager::Get()->GetCrasAudioClient()->GetNodes(
      base::Bind(&CrasAudioSwitchHandler::HandleGetNodes,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioSwitchHandler::SwitchToDevice(const AudioDevice& device) {
  // Mute -> Switch active device -> (on active device changed event) Unmute.
  muted_device_id_ = device.id;
  LOG(WARNING) << "Switching active device to: " << device.ToString();
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

void CrasAudioSwitchHandler::UpdateDevicesAndSwitch(
    const AudioNodeList& nodes) {
  bool input_device_removed = false;
  bool output_device_removed = false;
  bool have_active_input_device = false;
  bool have_active_output_device = false;

  size_t num_previous_input_devices = input_devices_.size();
  size_t num_previous_output_devices = output_devices_.size();

  while (!input_devices_.empty())
    input_devices_.pop();
  while (!output_devices_.empty())
    output_devices_.pop();

  for (size_t i = 0; i < nodes.size(); ++i) {
    AudioDevice dev(nodes[i]);
    if (dev.is_input)
      input_devices_.push(dev);
    else
      output_devices_.push(dev);

    if (dev.is_input && dev.active)
      have_active_input_device = true;
    if (!dev.is_input && dev.active)
      have_active_output_device = true;
  }

  if (num_previous_input_devices > input_devices_.size())
    input_device_removed = true;
  if (num_previous_output_devices > output_devices_.size())
    output_device_removed = true;

  // If either,
  // .) the top input/output device is already active, or,
  // .) an input/output device was removed but not the active device,
  // then we don't need to switch the device, otherwise we do need to switch.
  if (!(input_devices_.top().active ||
      (input_device_removed && have_active_input_device))) {
    SwitchToDevice(input_devices_.top());
  }

  if (!(output_devices_.top().active ||
      (output_device_removed && have_active_output_device))) {
    SwitchToDevice(output_devices_.top());
  }
}

void CrasAudioSwitchHandler::HandleGetNodes(
    const AudioNodeList& node_list, bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to retrieve audio nodes data";
    return;
  }

  UpdateDevicesAndSwitch(node_list);
}

}  // namespace chromeos
