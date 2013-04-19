// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_AUDIO_PREF_HANDLER_H_
#define CHROMEOS_AUDIO_AUDIO_PREF_HANDLER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chromeos/audio/audio_pref_observer.h"
#include "chromeos/chromeos_export.h"

class PrefRegistrySimple;

namespace chromeos {

// Interface that handles audio preference related work, reads and writes
// audio preferences, and notifies AudioPrefObserver for audio preference
// changes.
class CHROMEOS_EXPORT AudioPrefHandler
    : public base::RefCountedThreadSafe<AudioPrefHandler> {
 public:
  // Gets the audio output volume value from prefs.
  virtual double GetOutputVolumeValue() = 0;

  // Sets the output audio volume value to prefs.
  virtual void SetOutputVolumeValue(double volume_percent) = 0;

  // Reads the audio output mute value from prefs.
  virtual bool GetOutputMuteValue() = 0;

  // Sets the audio output mute value to prefs.
  virtual void SetOutputMuteValue(bool mute_on) = 0;

  // Reads the audio capture allowed value from prefs.
  virtual bool GetAudioCaptureAllowedValue() = 0;

  // Sets the audio output allowed value from prefs.
  virtual bool GetAudioOutputAllowedValue() = 0;

  // Adds an audio preference observer.
  virtual void AddAudioPrefObserver(AudioPrefObserver* observer) = 0;

  // Removes an audio preference observer.
  virtual void RemoveAudioPrefObserver(AudioPrefObserver* observer) = 0;

  // Creates the instance.
  static AudioPrefHandler* Create(PrefService* local_state);

 protected:
  virtual ~AudioPrefHandler() {}

 private:
  friend class base::RefCountedThreadSafe<AudioPrefHandler>;
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_AUDIO_PREF_HANDLER_H_
