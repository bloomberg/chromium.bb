// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_CRAS_AUDIO_HANDLER_H_
#define CHROMEOS_AUDIO_CRAS_AUDIO_HANDLER_H_

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

namespace chromeos {

class AudioPrefHandler;

class CHROMEOS_EXPORT CrasAudioHandler : public CrasAudioClient::Observer,
                                         public AudioPrefObserver {
 public:
  class AudioObserver {
   public:
    // Called when output volume changed.
    virtual void OnOutputVolumeChanged();

    // Called when output mute state changed.
    virtual void OnOutputMuteChanged();

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
  static void Initialize(scoped_refptr<AudioPrefHandler> audio_pref_handler);

  // Destroys the global instance.
  static void Shutdown();

  // Returns true if the global instance is initialized.
  static bool IsInitialized();

  // Gets the global instance. Initialize must be called first.
  static CrasAudioHandler* Get();

  // Adds an audio observer.
  void AddAudioObserver(AudioObserver* observer);

  // Removes an audio observer.
  void RemoveAudioObserver(AudioObserver* observer);

  // Returns true if audio output is muted.
  bool IsOutputMuted();

  // Returns true if audio input is muted.
  bool IsInputMuted();

  // Gets volume level in 0-100% range, 0 being pure silence.
  int GetOutputVolumePercent();

  // Returns node_id of the active output node.
  uint64 GetActiveOutputNode() const;

  // Returns the node_id of the active input node.
  uint64 GetActiveInputNode() const;

  // Gets the audio devices back in |device_list|.
  void GetAudioDevices(AudioDeviceList* device_list) const;

  bool GetActiveOutputDevice(AudioDevice* device) const;

  // Whether there is alternative input/output audio device.
  bool has_alternative_input() const { return has_alternative_input_; }
  bool has_alternative_output() const { return has_alternative_output_; }

  // Sets volume level from 0-100%. If less than kMuteThresholdPercent, then
  // mutes the sound. If it was muted, and |volume_percent| is larger than
  // the threshold, then the sound is unmuted.
  void SetOutputVolumePercent(int volume_percent);

  // Adjusts volume up (positive percentage) or down (negative percentage).
  void AdjustOutputVolumeByPercent(int adjust_by_percent);

  // Mutes or unmutes audio output device.
  void SetOutputMute(bool mute_on);

  // Mutes or unmutes audio input device.
  void SetInputMute(bool mute_on);

  // Sets the active audio output node to the node with |node_id|.
  void SetActiveOutputNode(uint64 node_id);

  // Sets the active audio input node to the node with |node_id|.
  void SetActiveInputNode(uint64 node_id);

 private:
  explicit CrasAudioHandler(scoped_refptr<AudioPrefHandler> audio_pref_handler);
  virtual ~CrasAudioHandler();

  // Overriden from CrasAudioHandler::Observer.
  virtual void AudioClientRestarted() OVERRIDE;
  virtual void OutputVolumeChanged(int volume) OVERRIDE;
  virtual void OutputMuteChanged(bool mute_on) OVERRIDE;
  virtual void InputMuteChanged(bool mute_on) OVERRIDE;
  virtual void NodesChanged() OVERRIDE;
  virtual void ActiveOutputNodeChanged(uint64 node_id) OVERRIDE;
  virtual void ActiveInputNodeChanged(uint64 node_id) OVERRIDE;

  // Overriden from AudioPrefObserver.
  virtual void OnAudioPolicyPrefChanged() OVERRIDE;

  // Sets up the initial audio device state based on audio policy and
  // audio settings saved in prefs.
  void SetupInitialAudioState();

  // Applies the audio muting policies whenever the user logs in or policy
  // change notification is received.
  void ApplyAudioPolicy();

  // Sets output volume to specified value and notifies observers.
  void SetOutputVolumeInternal(int volume);

  // Calling dbus to get nodes data.
  void GetNodes();

  // Handles dbus callback for GetNodes.
  void HandleGetNodes(const chromeos::AudioNodeList& node_list, bool success);

  scoped_refptr<AudioPrefHandler> audio_pref_handler_;
  base::WeakPtrFactory<CrasAudioHandler> weak_ptr_factory_;
  ObserverList<AudioObserver> observers_;

  // Audio data and state.
  AudioDeviceList audio_devices_;
  VolumeState volume_state_;
  bool output_mute_on_;
  bool input_mute_on_;
  int output_volume_;
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
