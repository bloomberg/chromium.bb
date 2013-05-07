// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_CRAS_AUDIO_SWITCH_HANDLER_H_
#define CHROMEOS_AUDIO_CRAS_AUDIO_SWITCH_HANDLER_H_

#include <queue>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/dbus/audio_node.h"
#include "chromeos/dbus/cras_audio_client.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

class AudioPrefHandler;

typedef std::priority_queue<AudioDevice,
                            std::vector<AudioDevice>,
                            AudioDeviceCompare> AudioDevicePriorityQueue;

// Class which handles switching between audio devices when a new device is
// plugged in or an active device is plugged out.
class CHROMEOS_EXPORT CrasAudioSwitchHandler
    : public CrasAudioClient::Observer {
 public:
  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize must be called first.
  static CrasAudioSwitchHandler* Get();

 private:
  explicit CrasAudioSwitchHandler();
  virtual ~CrasAudioSwitchHandler();

  // Overriden from CrasAudioSwitchHandler::Observer.
  virtual void AudioClientRestarted() OVERRIDE;
  virtual void NodesChanged() OVERRIDE;
  virtual void ActiveOutputNodeChanged(uint64 node_id) OVERRIDE;
  virtual void ActiveInputNodeChanged(uint64 node_id) OVERRIDE;

  // Sets up the initial audio device state based on audio policy and
  // audio settings saved in prefs.
  void SetupInitialAudioState();

  // Calling dbus to get nodes data.
  void GetNodes();

  // Updates the current audio nodes list and switches our active device
  // if needed.
  void UpdateDevicesAndSwitch(const AudioNodeList& nodes);

  void SwitchToDevice(const AudioDevice& device);

  // Handles dbus callback for GetNodes.
  void HandleGetNodes(const chromeos::AudioNodeList& node_list, bool success);

  // Audio data and state.
  AudioDevicePriorityQueue input_devices_;
  AudioDevicePriorityQueue output_devices_;
  uint64 muted_device_id_;

  base::WeakPtrFactory<CrasAudioSwitchHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrasAudioSwitchHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_CRAS_AUDIO_SWITCH_HANDLER_H_
