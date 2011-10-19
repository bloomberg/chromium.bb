// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_SPEECH_SYNTHESIZER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_SPEECH_SYNTHESIZER_CLIENT_H_
#pragma once

#include <string>

#include "base/callback.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// SpeechSynthesizerClient is used to communicate with the speech synthesizer.
// All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class SpeechSynthesizerClient {
 public:
  // A callback function called when the result of IsSpeaking is ready.
  // The argument indicates if the speech synthesizer is speaking or not.
  typedef base::Callback<void(bool)> IsSpeakingCallback;

  virtual ~SpeechSynthesizerClient() {}

  // Speaks the specified text.
  virtual void Speak(const std::string& text) = 0;

  // Sets options for the subsequent speech synthesis requests.
  // Use the constants below.
  virtual void SetSpeakProperties(const std::string& props) = 0;

  // Stops speaking the current utterance.
  virtual void StopSpeaking() = 0;

  // Checks if the engine is currently speaking.
  // |callback| will be called on the origin thread later with the result.
  virtual void IsSpeaking(IsSpeakingCallback callback) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static SpeechSynthesizerClient* Create(dbus::Bus* bus);

  // Constants to be used with SetSpeakProperties.
  static const char kSpeechPropertyLocale[];
  static const char kSpeechPropertyGender[];
  static const char kSpeechPropertyRate[];
  static const char kSpeechPropertyPitch[];
  static const char kSpeechPropertyVolume[];
  static const char kSpeechPropertyEquals[];
  static const char kSpeechPropertyDelimiter[];
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_SPEECH_SYNTHESIZER_CLIENT_H_
