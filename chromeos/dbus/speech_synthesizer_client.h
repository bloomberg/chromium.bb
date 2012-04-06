// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SPEECH_SYNTHESIZER_CLIENT_H_
#define CHROMEOS_DBUS_SPEECH_SYNTHESIZER_CLIENT_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// SpeechSynthesizerClient is used to communicate with the speech synthesizer.
// All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT SpeechSynthesizerClient {
 public:
  // A callback function called when the result of IsSpeaking is ready.
  // The argument indicates if the speech synthesizer is speaking or not.
  typedef base::Callback<void(bool)> IsSpeakingCallback;

  virtual ~SpeechSynthesizerClient();

  // Speaks the specified text with properties.
  // Use the constants below for properties.
  // An example of |properties|: "rate=1.0 pitch=1.0"
  virtual void Speak(const std::string& text,
                     const std::string& properties) = 0;

  // Stops speaking the current utterance.
  virtual void StopSpeaking() = 0;

  // Checks if the engine is currently speaking.
  // |callback| will be called on the origin thread later with the result.
  virtual void IsSpeaking(const IsSpeakingCallback& callback) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static SpeechSynthesizerClient* Create(DBusClientImplementationType type,
                                         dbus::Bus* bus);

  // Constants to be used with the properties argument to Speak.
  static const char kSpeechPropertyLocale[];
  static const char kSpeechPropertyGender[];
  static const char kSpeechPropertyRate[];
  static const char kSpeechPropertyPitch[];
  static const char kSpeechPropertyVolume[];
  static const char kSpeechPropertyEquals[];
  static const char kSpeechPropertyDelimiter[];

 protected:
  // Create() should be used instead.
  SpeechSynthesizerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(SpeechSynthesizerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SPEECH_SYNTHESIZER_CLIENT_H_
