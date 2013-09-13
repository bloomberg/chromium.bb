// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CRAS_AUDIO_CLIENT_STUB_IMPL_H_
#define CHROMEOS_DBUS_CRAS_AUDIO_CLIENT_STUB_IMPL_H_

#include "chromeos/dbus/cras_audio_client.h"

namespace chromeos {

class CrasAudioHandlerTest;

// The CrasAudioClient implementation used on Linux desktop.
class CrasAudioClientStubImpl : public CrasAudioClient {
 public:
  CrasAudioClientStubImpl();
  virtual ~CrasAudioClientStubImpl();

  // CrasAudioClient overrides
  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual bool HasObserver(Observer* observer) OVERRIDE;
  virtual void GetVolumeState(const GetVolumeStateCallback& callback) OVERRIDE;
  virtual void GetNodes(const GetNodesCallback& callback,
                        const ErrorCallback& error_callback) OVERRIDE;
  virtual void SetOutputNodeVolume(uint64 node_id, int32 volume) OVERRIDE;
  virtual void SetOutputUserMute(bool mute_on) OVERRIDE;
  virtual void SetInputNodeGain(uint64 node_id, int32 gain) OVERRIDE;
  virtual void SetInputMute(bool mute_on) OVERRIDE;
  virtual void SetActiveOutputNode(uint64 node_id) OVERRIDE;
  virtual void SetActiveInputNode(uint64 node_id) OVERRIDE;

 protected:
  // Helper functions for testing
  virtual void SetAudioDevices(const AudioNodeList& audio_nodes);
  virtual void ChangeAudioNodes(const AudioNodeList& new_nodes);

 private:
  friend class CrasAudioHandlerTest;

  VolumeState volume_state_;
  AudioNodeList node_list_;
  uint64 active_input_node_id_;
  uint64 active_output_node_id_;
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(CrasAudioClientStubImpl);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CRAS_AUDIO_CLIENT_STUB_IMPL_H_
