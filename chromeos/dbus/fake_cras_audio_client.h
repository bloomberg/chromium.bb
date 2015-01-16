// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_CRAS_AUDIO_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_CRAS_AUDIO_CLIENT_H_

#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/cras_audio_client.h"

namespace chromeos {

class CrasAudioHandlerTest;

// The CrasAudioClient implementation used on Linux desktop.
class CHROMEOS_EXPORT FakeCrasAudioClient : public CrasAudioClient {
 public:
  FakeCrasAudioClient();
  ~FakeCrasAudioClient() override;

  // CrasAudioClient overrides:
  void Init(dbus::Bus* bus) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  void GetVolumeState(const GetVolumeStateCallback& callback) override;
  void GetNodes(const GetNodesCallback& callback,
                const ErrorCallback& error_callback) override;
  void SetOutputNodeVolume(uint64 node_id, int32 volume) override;
  void SetOutputUserMute(bool mute_on) override;
  void SetInputNodeGain(uint64 node_id, int32 gain) override;
  void SetInputMute(bool mute_on) override;
  void SetActiveOutputNode(uint64 node_id) override;
  void SetActiveInputNode(uint64 node_id) override;
  void AddActiveInputNode(uint64 node_id) override;
  void RemoveActiveInputNode(uint64 node_id) override;
  void AddActiveOutputNode(uint64 node_id) override;
  void RemoveActiveOutputNode(uint64 node_id) override;
  void SwapLeftRight(uint64 node_id, bool swap) override;

  // Updates |node_list_| to contain |audio_nodes|.
  void SetAudioNodesForTesting(const AudioNodeList& audio_nodes);

  // Calls SetAudioNodesForTesting() and additionally notifies |observers_|.
  void SetAudioNodesAndNotifyObserversForTesting(
      const AudioNodeList& new_nodes);

 private:
  VolumeState volume_state_;
  AudioNodeList node_list_;
  uint64 active_input_node_id_;
  uint64 active_output_node_id_;
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeCrasAudioClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_CRAS_AUDIO_CLIENT_H_
