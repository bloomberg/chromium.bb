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
#include "chromeos/dbus/volume_state.h"

class PrefRegistrySimple;
class PrefService;

namespace {

// Default value for the volume pref, as a percent in the range [0.0, 100.0].
const double kDefaultVolumeGainPercent = 75.0;

// Values used for muted preference.
const int kPrefMuteOff = 0;
const int kPrefMuteOn = 1;

}  // namespace

namespace chromeos {

class AudioDevicesPrefHandler;

class CHROMEOS_EXPORT CrasAudioHandler : public CrasAudioClient::Observer,
                                         public AudioPrefObserver {
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

  // Returns true if audio output is muted.
  virtual bool IsOutputMuted();

  // Returns true if audio output is muted for a device.
  virtual bool IsOutputMutedForDevice(uint64 device_id);

  // Returns true if audio input is muted.
  virtual bool IsInputMuted();

  // Returns true if audio input is muted for a device.
  virtual bool IsInputMutedForDevice(uint64 device_id);

  // Returns true if the output volume is below the default mute volume level.
  virtual bool IsOutputVolumeBelowDefaultMuteLvel();

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

  // Sets volume level from 0-100%. If less than kMuteThresholdPercent, then
  // mutes the sound. If it was muted, and |volume_percent| is larger than
  // the threshold, then the sound is unmuted.
  virtual void SetOutputVolumePercent(int volume_percent);

  // Sets gain level from 0-100%.
  virtual void SetInputGainPercent(int gain_percent);

  // Adjusts volume up (positive percentage) or down (negative percentage).
  virtual void AdjustOutputVolumeByPercent(int adjust_by_percent);

  // Adjusts output volume to a minimum audible level if it is too low.
  virtual void AdjustOutputVolumeToAudibleLevel();

  // Mutes or unmutes audio output device.
  virtual void SetOutputMute(bool mute_on);

  // Mutes or unmutes audio input device.
  virtual void SetInputMute(bool mute_on);

  // Sets the active audio output node to the node with |node_id|.
  virtual void SetActiveOutputNode(uint64 node_id);

  // Sets the active audio input node to the node with |node_id|.
  virtual void SetActiveInputNode(uint64 node_id);

  // Sets volume/gain level for a device.
  virtual void SetVolumeGainPercentForDevice(uint64 device_id, int value);

  // Sets the mute for device.
  virtual void SetMuteForDevice(uint64 device_id, bool mute_on);

 protected:
  explicit CrasAudioHandler(
      scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler);
  virtual ~CrasAudioHandler();

 private:
  // Overriden from CrasAudioClient::Observer.
  virtual void AudioClientRestarted() OVERRIDE;
  virtual void OutputVolumeChanged(int volume) OVERRIDE;
  virtual void OutputMuteChanged(bool mute_on) OVERRIDE;
  virtual void InputGainChanged(int gain) OVERRIDE;
  virtual void InputMuteChanged(bool mute_on) OVERRIDE;
  virtual void NodesChanged() OVERRIDE;
  virtual void ActiveOutputNodeChanged(uint64 node_id) OVERRIDE;
  virtual void ActiveInputNodeChanged(uint64 node_id) OVERRIDE;

  // Overriden from AudioPrefObserver.
  virtual void OnAudioPolicyPrefChanged() OVERRIDE;

  // Sets up the audio device state based on audio policy and audio settings
  // saved in prefs.
  void SetupAudioInputState();
  void SetupAudioOutputState();

  const AudioDevice* GetDeviceFromId(uint64 device_id) const;

  // Initializes audio state, which should only be called when CrasAudioHandler
  // is created or cras audio client is restarted.
  void InitializeAudioState();

  // Applies the audio muting policies whenever the user logs in or policy
  // change notification is received.
  void ApplyAudioPolicy();

  // Sets output volume to specified value of |volume|.
  void SetOutputVolumeInternal(int volume);

  // Sets output mute state to |mute_on| internally, returns true if output mute
  // is set.
  bool SetOutputMuteInternal(bool mute_on);

  // Sets output volume to specified value and notifies observers.
  void SetInputGainInternal(int gain);

  // Sets input mute state to |mute_on| internally, returns true if input mute
  // is set.
  bool SetInputMuteInternal(bool mute_on);

  // Calling dbus to get nodes data.
  void GetNodes();

  // Updates the current audio nodes list and switches the active device
  // if needed.
  void UpdateDevicesAndSwitchActive(const AudioNodeList& nodes);

  void SwitchToDevice(const AudioDevice& device);

  // Handles dbus callback for GetNodes.
  void HandleGetNodes(const chromeos::AudioNodeList& node_list, bool success);

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

  DISALLOW_COPY_AND_ASSIGN(CrasAudioHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_CRAS_AUDIO_HANDLER_H_
