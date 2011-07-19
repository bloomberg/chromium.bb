// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SPEECH_SYNTHESIS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SPEECH_SYNTHESIS_LIBRARY_H_
#pragma once

#include "base/memory/singleton.h"

namespace chromeos {

// This interface defines the interaction with the ChromeOS login library APIs.
class SpeechSynthesisLibrary {
 public:
  typedef void(*InitStatusCallback)(bool success);

  virtual ~SpeechSynthesisLibrary() {}

  // Speaks the specified text.
  virtual bool Speak(const char* text) = 0;

  // Sets options for the subsequent speech synthesis requests.
  // Use the constants below.
  virtual bool SetSpeakProperties(const char* props) = 0;

  // Stops speaking the current utterance.
  virtual bool StopSpeaking() = 0;

  // Checks if the engine is currently speaking.
  virtual bool IsSpeaking() = 0;

  // Starts the speech synthesis service and indicates through a callback if
  // it started successfully.
  virtual void InitTts(InitStatusCallback) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static SpeechSynthesisLibrary* GetImpl(bool stub);

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

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SPEECH_SYNTHESIS_LIBRARY_H_
