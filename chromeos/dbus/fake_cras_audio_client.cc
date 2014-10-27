// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cras_audio_client.h"

namespace chromeos {

FakeCrasAudioClient::FakeCrasAudioClient()
    : active_input_node_id_(0),
      active_output_node_id_(0) {
}

FakeCrasAudioClient::~FakeCrasAudioClient() {
}

void FakeCrasAudioClient::Init(dbus::Bus* bus) {
  VLOG(1) << "FakeCrasAudioClient is created";

  // Fake audio output nodes.
  AudioNode node_1;
  node_1.is_input = false;
  node_1.id = 10001;
  node_1.device_name = "Fake Speaker";
  node_1.type = "INTERNAL_SPEAKER";
  node_1.name = "Speaker";
  node_list_.push_back(node_1);

  AudioNode node_2;
  node_2.is_input = false;
  node_2.id = 10002;
  node_2.device_name = "Fake Headphone";
  node_2.type = "HEADPHONE";
  node_2.name = "Headphone";
  node_list_.push_back(node_2);

  AudioNode node_3;
  node_3.is_input = false;
  node_3.id = 10003;
  node_3.device_name = "Fake Bluetooth Headphone";
  node_3.type = "BLUETOOTH";
  node_3.name = "Headphone";
  node_list_.push_back(node_3);

  // Fake audio input ndoes
  AudioNode node_4;
  node_4.is_input = true;
  node_4.id = 10004;
  node_4.device_name = "Fake Internal Mic";
  node_4.type = "INTERNAL_MIC";
  node_4.name = "Internal Mic";
  node_list_.push_back(node_4);

  AudioNode node_5;
  node_5.is_input = true;
  node_5.id = 10005;
  node_5.device_name = "Fake USB Mic";
  node_5.type = "USB";
  node_5.name = "Mic";
  node_list_.push_back(node_5);
}

void FakeCrasAudioClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeCrasAudioClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeCrasAudioClient::HasObserver(Observer* observer) {
  return observers_.HasObserver(observer);
}

void FakeCrasAudioClient::GetVolumeState(
    const GetVolumeStateCallback& callback) {
  callback.Run(volume_state_, true);
}

void FakeCrasAudioClient::GetNodes(const GetNodesCallback& callback,
                                   const ErrorCallback& error_callback) {
  callback.Run(node_list_, true);
}

void FakeCrasAudioClient::SetOutputNodeVolume(uint64 node_id,
                                              int32 volume) {
}

void FakeCrasAudioClient::SetOutputUserMute(bool mute_on) {
  volume_state_.output_user_mute = mute_on;
  FOR_EACH_OBSERVER(Observer,
                    observers_,
                    OutputMuteChanged(volume_state_.output_user_mute));
}

void FakeCrasAudioClient::SetInputNodeGain(uint64 node_id,
                                           int32 input_gain) {
}

void FakeCrasAudioClient::SetInputMute(bool mute_on) {
  volume_state_.input_mute = mute_on;
  FOR_EACH_OBSERVER(Observer,
                    observers_,
                    InputMuteChanged(volume_state_.input_mute));
}

void FakeCrasAudioClient::SetActiveOutputNode(uint64 node_id) {
  if (active_output_node_id_ == node_id)
    return;

  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == active_output_node_id_)
      node_list_[i].active = false;
    else if (node_list_[i].id == node_id)
      node_list_[i].active = true;
  }
  active_output_node_id_ = node_id;
  FOR_EACH_OBSERVER(Observer,
                    observers_,
                    ActiveOutputNodeChanged(node_id));
}

void FakeCrasAudioClient::SetActiveInputNode(uint64 node_id) {
  if (active_input_node_id_ == node_id)
    return;

  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == active_input_node_id_)
      node_list_[i].active = false;
    else if (node_list_[i].id == node_id)
      node_list_[i].active = true;
  }
  active_input_node_id_ = node_id;
  FOR_EACH_OBSERVER(Observer,
                    observers_,
                    ActiveInputNodeChanged(node_id));
}

void FakeCrasAudioClient::AddActiveInputNode(uint64 node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id)
      node_list_[i].active = true;
  }
}

void FakeCrasAudioClient::RemoveActiveInputNode(uint64 node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id)
      node_list_[i].active = false;
  }
}

void FakeCrasAudioClient::SwapLeftRight(uint64 node_id, bool swap) {
}

void FakeCrasAudioClient::AddActiveOutputNode(uint64 node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id)
      node_list_[i].active = true;
  }
}

void FakeCrasAudioClient::RemoveActiveOutputNode(uint64 node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id)
      node_list_[i].active = false;
  }
}

void FakeCrasAudioClient::SetAudioNodesForTesting(
    const AudioNodeList& audio_nodes) {
  node_list_ = audio_nodes;
}

void FakeCrasAudioClient::SetAudioNodesAndNotifyObserversForTesting(
    const AudioNodeList& new_nodes) {
  SetAudioNodesForTesting(new_nodes);
  FOR_EACH_OBSERVER(Observer, observers_, NodesChanged());
}

}  // namespace chromeos
