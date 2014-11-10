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
  virtual ~FakeCrasAudioClient();

  // CrasAudioClient overrides:
  virtual void Init(dbus::Bus* bus) override;
  virtual void AddObserver(Observer* observer) override;
  virtual void RemoveObserver(Observer* observer) override;
  virtual bool HasObserver(const Observer* observer) const override;
  virtual void GetVolumeState(const GetVolumeStateCallback& callback) override;
  virtual void GetNodes(const GetNodesCallback& callback,
                        const ErrorCallback& error_callback) override;
  virtual void SetOutputNodeVolume(uint64 node_id, int32 volume) override;
  virtual void SetOutputUserMute(bool mute_on) override;
  virtual void SetInputNodeGain(uint64 node_id, int32 gain) override;
  virtual void SetInputMute(bool mute_on) override;
  virtual void SetActiveOutputNode(uint64 node_id) override;
  virtual void SetActiveInputNode(uint64 node_id) override;
  virtual void AddActiveInputNode(uint64 node_id) override;
  virtual void RemoveActiveInputNode(uint64 node_id) override;
  virtual void AddActiveOutputNode(uint64 node_id) override;
  virtual void RemoveActiveOutputNode(uint64 node_id) override;
  virtual void SwapLeftRight(uint64 node_id, bool swap) override;

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
