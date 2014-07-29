// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_CRAS_AUDIO_HANDLER_H_
#define CHROMEOS_AUDIO_CRAS_AUDIO_HANDLER_H_

#include <queue>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/audio_pref_observer.h"
#include "chromeos/dbus/audio_node.h"
#include "chromeos/dbus/cras_audio_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/volume_state.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

class AudioDevicesPrefHandler;

class CHROMEOS_EXPORT CrasAudioHandler : public CrasAudioClient::Observer,
                                         public AudioPrefObserver,
                                         public SessionManagerClient::Observer {
 public:
  typedef std::priority_queue<AudioDevice,
                              std::vector<AudioDevice>,
                              AudioDeviceCompare> AudioDevicePriorityQueue;

  class AudioObserver {
   public:
    // Called when output volume changed.
    virtual void OnOutputVolumeChanged();

    // Called when output mute state changed.
    virtual void OnOutputMuteChanged();

    // Called when input mute state changed.
    virtual void OnInputGainChanged();

    // Called when input mute state changed.
    virtual void OnInputMuteChanged();

    // Called when audio nodes changed.
    virtual void OnAudioNodesChanged();

    // Called when active audio node changed.
    virtual void OnActiveOutputNodeChanged();

    // Called when active audio input node changed.
    virtual void OnActiveInputNodeChanged();

   protected:
    AudioObserver();
    virtual ~AudioObserver();
    DISALLOW_COPY_AND_ASSIGN(AudioObserver);
  };

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize(
      scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler);

  // Sets the global instance for testing.
  static void InitializeForTesting();

  // Destroys the global instance.
  static void Shutdown();

  // Returns true if the global instance is initialized.
  static bool IsInitialized();

  // Gets the global instance. Initialize must be called first.
  static CrasAudioHandler* Get();

  // Adds an audio observer.
  virtual void AddAudioObserver(AudioObserver* observer);

  // Removes an audio observer.
  virtual void RemoveAudioObserver(AudioObserver* observer);

  // Returns true if keyboard mic exists.
  virtual bool HasKeyboardMic();

  // Returns true if audio output is muted.
  virtual bool IsOutputMuted();

  // Returns true if audio output is muted for a device.
  virtual bool IsOutputMutedForDevice(uint64 device_id);

  // Returns true if audio input is muted.
  virtual bool IsInputMuted();

  // Returns true if audio input is muted for a device.
  virtual bool IsInputMutedForDevice(uint64 device_id);

  // Returns true if the output volume is below the default mute volume level.
  virtual bool IsOutputVolumeBelowDefaultMuteLevel();

  // Returns volume level in 0-100% range at which the volume should be muted.
  virtual int GetOutputDefaultVolumeMuteThreshold();

  // Gets volume level in 0-100% range (0 being pure silence) for the current
  // active node.
  virtual int GetOutputVolumePercent();

  // Gets volume level in 0-100% range (0 being pure silence) for a device.
  virtual int GetOutputVolumePercentForDevice(uint64 device_id);

  // Gets gain level in 0-100% range (0 being pure silence) for the current
  // active node.
  virtual int GetInputGainPercent();

  // Gets volume level in 0-100% range (0 being pure silence) for a device.
  virtual int GetInputGainPercentForDevice(uint64 device_id);

  // Returns node_id of the active output node.
  virtual uint64 GetActiveOutputNode() const;

  // Returns the node_id of the active input node.
  virtual uint64 GetActiveInputNode() const;

  // Gets the audio devices back in |device_list|.
  virtual void GetAudioDevices(AudioDeviceList* device_list) const;

  virtual bool GetActiveOutputDevice(AudioDevice* device) const;

  // Whether there is alternative input/output audio device.
  virtual bool has_alternative_input() const;
  virtual bool has_alternative_output() const;

  // Sets volume level to |volume_percent|, whose range is from 0-100%.
  virtual void SetOutputVolumePercent(int volume_percent);

  // Sets gain level to |gain_percent|, whose range is from 0-100%.
  virtual void SetInputGainPercent(int gain_percent);

  // Adjusts volume up (positive percentage) or down (negative percentage).
  virtual void AdjustOutputVolumeByPercent(int adjust_by_percent);

  // Adjusts output volume to a minimum audible level if it is too low.
  virtual void AdjustOutputVolumeToAudibleLevel();

  // Mutes or unmutes audio output device.
  virtual void SetOutputMute(bool mute_on);

  // Mutes or unmutes audio input device.
  virtual void SetInputMute(bool mute_on);

  // Switches active audio device to |device|.
  virtual void SwitchToDevice(const AudioDevice& device);

  // Sets volume/gain level for a device.
  virtual void SetVolumeGainPercentForDevice(uint64 device_id, int value);

  // Sets the mute for device.
  virtual void SetMuteForDevice(uint64 device_id, bool mute_on);

  // Activates or deactivates keyboard mic if there's one.
  virtual void SetKeyboardMicActive(bool active);

  // Enables error logging.
  virtual void LogErrors();

 protected:
  explicit CrasAudioHandler(
      scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler);
  virtual ~CrasAudioHandler();

 private:
  // CrasAudioClient::Observer overrides.
  virtual void AudioClientRestarted() OVERRIDE;
  virtual void NodesChanged() OVERRIDE;
  virtual void ActiveOutputNodeChanged(uint64 node_id) OVERRIDE;
  virtual void ActiveInputNodeChanged(uint64 node_id) OVERRIDE;

  // AudioPrefObserver overrides.
  virtual void OnAudioPolicyPrefChanged() OVERRIDE;

  // SessionManagerClient::Observer overrides.
  virtual void EmitLoginPromptVisibleCalled() OVERRIDE;

  // Sets the active audio output/input node to the node with |node_id|.
  void SetActiveOutputNode(uint64 node_id);
  void SetActiveInputNode(uint64 node_id);

  // Sets up the audio device state based on audio policy and audio settings
  // saved in prefs.
  void SetupAudioInputState();
  void SetupAudioOutputState();

  const AudioDevice* GetDeviceFromId(uint64 device_id) const;
  const AudioDevice* GetKeyboardMic() const;

  // Initializes audio state, which should only be called when CrasAudioHandler
  // is created or cras audio client is restarted.
  void InitializeAudioState();

  // Applies the audio muting policies whenever the user logs in or policy
  // change notification is received.
  void ApplyAudioPolicy();

  // Sets output volume of |node_id| to |volume|.
  void SetOutputNodeVolume(uint64 node_id, int volume);

  // Sets output mute state to |mute_on| internally, returns true if output mute
  // is set.
  bool SetOutputMuteInternal(bool mute_on);

  // Sets input gain of |node_id| to |gain|.
  void SetInputNodeGain(uint64 node_id, int gain);

  // Sets input mute state to |mute_on| internally, returns true if input mute
  // is set.
  bool SetInputMuteInternal(bool mute_on);

  // Calling dbus to get nodes data.
  void GetNodes();

  // Updates the current audio nodes list and switches the active device
  // if needed.
  void UpdateDevicesAndSwitchActive(const AudioNodeList& nodes);

  // Returns true if *|current_active_node_id| device is changed to
  // |new_active_device|.
  bool ChangeActiveDevice(const AudioDevice& new_active_device,
                          uint64* current_active_node_id);

  // Returns true if the audio nodes change is caused by some non-active
  // audio nodes unplugged.
  bool NonActiveDeviceUnplugged(size_t old_devices_size,
                                size_t new_device_size,
                                uint64 current_active_node);

  // Returns true if there is any device change for for input or output,
  // specified by |is_input|.
  bool HasDeviceChange(const AudioNodeList& new_nodes, bool is_input);

  // Handles dbus callback for GetNodes.
  void HandleGetNodes(const chromeos::AudioNodeList& node_list, bool success);

  // Handles the dbus error callback.
  void HandleGetNodesError(const std::string& error_name,
                           const std::string& error_msg);

  // Returns true if |device| is not found in audio_devices_.
  bool FoundNewDevice(const AudioDevice& device);

  // Returns a sanitized AudioDevice from |node|.
  AudioDevice GetSanitizedAudioDevice(const AudioNode& node);

  scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler_;
  base::WeakPtrFactory<CrasAudioHandler> weak_ptr_factory_;
  ObserverList<AudioObserver> observers_;

  // Audio data and state.
  AudioDeviceMap audio_devices_;

  AudioDevicePriorityQueue input_devices_pq_;
  AudioDevicePriorityQueue output_devices_pq_;

  bool output_mute_on_;
  bool input_mute_on_;
  int output_volume_;
  int input_gain_;
  uint64 active_output_node_id_;
  uint64 active_input_node_id_;
  bool has_alternative_input_;
  bool has_alternative_output_;

  bool output_mute_locked_;
  bool input_mute_locked_;

  // Failures are not logged at startup, since CRAS may not be running yet.
  bool log_errors_;

  DISALLOW_COPY_AND_ASSIGN(CrasAudioHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_CRAS_AUDIO_HANDLER_H_
