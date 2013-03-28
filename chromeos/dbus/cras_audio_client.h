// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CRAS_AUDIO_CLIENT_H_
#define CHROMEOS_DBUS_CRAS_AUDIO_CLIENT_H_

#include "base/callback.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/volume_state.h"

namespace dbus {
class Bus;
}  // namespace

namespace chromeos {

// CrasAudioClient is used to communicate with the cras audio dbus interface.
class CHROMEOS_EXPORT CrasAudioClient {
 public:
  // Interface for observing changes from the cras audio changes.
  class Observer {
   public:
    // Called when audio output device volume changed.
    virtual void OutputVolumeChanged(int volume);
    virtual void OutputMuteChanged(bool mute_on);
    virtual void InputGainChanged(int gain);
    virtual void InputMuteChanged(bool mute_on);
    virtual void NodesChanged();
    virtual void ActiveOutputNodeChanged(uint64 node_id);
    virtual void ActiveInputNodeChanged(uint64 node_id);

   protected:
    virtual ~Observer();
  };

  virtual ~CrasAudioClient();

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  // Returns true if this object has the given observer.
  virtual bool HasObserver(Observer* observer) = 0;

  // GetVolumeStateCallback is used for GetVolumeState method. It receives
  // 1 argument, |volume_state| which containing both input and  output volume
  // state data.
  typedef base::Callback<void(const VolumeState&)> GetVolumeStateCallback;

  // Gets the volume state, asynchronously.
  virtual void GetVolumeState(const GetVolumeStateCallback& callback) = 0;

  // Sets output volume to |volume|, in the range of [0, 100].
  virtual void  SetOutputVolume(int32 volume) = 0;

  // Sets output mute state to |mute_on| value.
  virtual void SetOutputMute(bool mute_on) = 0;

  // Sets input gain to |input_gain|. |input_gain| is specified in dBFS * 100.
  virtual void SetInputGain(int32 input_gain) = 0;

  // Sets input mute state to |mute_on| value.
  virtual void SetInputMute(bool mute_on) = 0;

  // Sets the active output noe to |node_id|.
  virtual void SetActiveOutputNode(uint64 node_id) = 0;

  // Sets the active input noe to |node_id|.
  virtual void SetActiveInputNode(uint64 node_id) = 0;

  // Creates the instance.
  static CrasAudioClient* Create(DBusClientImplementationType type,
                                 dbus::Bus* bus);

 protected:
  // Create() should be used instead.
  CrasAudioClient();

 private:

  DISALLOW_COPY_AND_ASSIGN(CrasAudioClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CRAS_AUDIO_CLIENT_H_
