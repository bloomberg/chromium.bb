// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CRAS_AUDIO_CLIENT_H_
#define CHROMEOS_DBUS_CRAS_AUDIO_CLIENT_H_

#include "base/callback.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/audio_node.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/volume_state.h"

namespace chromeos {

// CrasAudioClient is used to communicate with the cras audio dbus interface.
class CHROMEOS_EXPORT CrasAudioClient : public DBusClient {
 public:
  // Interface for observing changes from the cras audio changes.
  class Observer {
   public:
    // Called when cras audio client starts or re-starts, which happens when
    // cros device powers up or restarted.
    virtual void AudioClientRestarted();

    // Called when audio output mute state changed to new state of |mute_on|.
    virtual void OutputMuteChanged(bool mute_on);

    // Called when audio input mute state changed to new state of |mute_on|.
    virtual void InputMuteChanged(bool mute_on);

    // Called when audio nodes change.
    virtual void NodesChanged();

    // Called when active audio output node changed to new node with |node_id|.
    virtual void ActiveOutputNodeChanged(uint64 node_id);

    // Called when active audio input node changed to new node with |node_id|.
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
  // 2 arguments, |volume_state| which containing both input and  output volume
  // state data, and |success| which indicates whether or not the request
  // succeeded.
  typedef base::Callback<void(const VolumeState&, bool)> GetVolumeStateCallback;

  // GetNodesCallback is used for GetNodes method. It receives 2 arguments,
  // |audio_nodes| which containing a list of audio nodes data and
  // |success| which indicates whether or not the request succeeded.
  typedef base::Callback<void(const AudioNodeList&, bool)> GetNodesCallback;

  // ErrorCallback is used for cras dbus method error response. It receives 2
  // arguments, |error_name| indicates the dbus error name, and |error_message|
  // contains the detailed dbus error message.
  typedef base::Callback<void(const std::string&,
                              const std::string&)> ErrorCallback;

  // Gets the volume state, asynchronously.
  virtual void GetVolumeState(const GetVolumeStateCallback& callback) = 0;

  // Gets an array of audio input and output nodes.
  virtual void GetNodes(const GetNodesCallback& callback,
                        const ErrorCallback& error_callback) = 0;

  // Sets output volume of the given |node_id| to |volume|, in the rage of
  // [0, 100].
  virtual void SetOutputNodeVolume(uint64 node_id, int32 volume) = 0;

  // Sets output mute from user action.
  virtual void SetOutputUserMute(bool mute_on) = 0;

  // Sets input gain of the given |node_id| to |gain|, in the range of
  // [0, 100].
  virtual void SetInputNodeGain(uint64 node_id, int32 gain) = 0;

  // Sets input mute state to |mute_on| value.
  virtual void SetInputMute(bool mute_on) = 0;

  // Sets the active output node to |node_id|.
  virtual void SetActiveOutputNode(uint64 node_id) = 0;

  // Sets the primary active input node to |node_id|.
  virtual void SetActiveInputNode(uint64 node_id) = 0;

  // Adds input node |node_id| to the active input list. This is used to add
  // an additional active input node besides the one set by SetActiveInputNode.
  // Note that this action will not trigger an ActiveInputNodeChanged event and
  // nothing will happen if the |node_id| has already been set as active.
  virtual void AddActiveInputNode(uint64 node_id) = 0;

  // Removes input node |node_id| from the active input list. This is used for
  // removing an active input node added by AddActiveInputNode.
  virtual void RemoveActiveInputNode(uint64 node_id) = 0;

  // Adds input node |node_id| to the active outputs list. This is used to add
  // an additional active output node besides the one set by SetActiveInputNode.
  // Note that this action will not trigger an ActiveOutputNodeChanged event
  // and nothing will happen if the |node_id| has already been set as active.
  virtual void AddActiveOutputNode(uint64 node_id) = 0;

  // Removes output node |node_id| from the active output list. This is used for
  // removing an active output node added by AddActiveOutputNode.
  virtual void RemoveActiveOutputNode(uint64 node_id) = 0;

  // Creates the instance.
  static CrasAudioClient* Create();

 protected:
  // Create() should be used instead.
  CrasAudioClient();

 private:

  DISALLOW_COPY_AND_ASSIGN(CrasAudioClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CRAS_AUDIO_CLIENT_H_
