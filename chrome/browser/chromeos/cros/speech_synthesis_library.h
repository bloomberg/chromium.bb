// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SPEECH_SYNTHESIS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SPEECH_SYNTHESIS_LIBRARY_H_

#include "base/singleton.h"

namespace chromeos {

// This interface defines the interaction with the ChromeOS login library APIs.
class SpeechSynthesisLibrary {
 public:
  typedef void(*InitStatusCallback)(bool success);

  virtual ~SpeechSynthesisLibrary() {}
  // Speaks the specified text.
  virtual bool Speak(const char* text) = 0;
  // Sets options for the subsequent speech synthesis requests.
  virtual bool SetSpeakProperties(const char* props) = 0;
  // Stops speaking the current utterance.
  virtual bool StopSpeaking() = 0;
  // Checks if the engine is currently speaking.
  virtual bool IsSpeaking() = 0;
  // Starts the speech synthesis service and indicates through a callback if
  // it started successfully.
  virtual void InitTts(InitStatusCallback) = 0;
};

// This class handles the interaction with the ChromeOS login library APIs.
class SpeechSynthesisLibraryImpl : public SpeechSynthesisLibrary {
 public:
  SpeechSynthesisLibraryImpl() {}
  virtual ~SpeechSynthesisLibraryImpl() {}

  // SpeechSynthesisLibrary overrides.
  virtual bool Speak(const char* text);
  virtual bool SetSpeakProperties(const char* props);
  virtual bool StopSpeaking();
  virtual bool IsSpeaking();
  virtual void InitTts(InitStatusCallback);

 private:
  DISALLOW_COPY_AND_ASSIGN(SpeechSynthesisLibraryImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SPEECH_SYNTHESIS_LIBRARY_H_
