// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_PLATFORM_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_PLATFORM_H_
#pragma once

#include <string>

#include "chrome/browser/speech/extension_api/tts_extension_api_controller.h"

// Abstract class that defines the native platform TTS interface,
// subclassed by specific implementations on Win, Mac, etc.
class ExtensionTtsPlatformImpl {
 public:
  static ExtensionTtsPlatformImpl* GetInstance();

  // Returns true if this platform implementation is supported and available.
  virtual bool PlatformImplAvailable() = 0;

  // Speak the given utterance with the given parameters if possible,
  // and return true on success. Utterance will always be nonempty.
  // If rate, pitch, or volume are -1.0, they will be ignored.
  //
  // The ExtensionTtsController will only try to speak one utterance at
  // a time. If it wants to interrupt speech, it will always call Stop
  // before speaking again.
  virtual bool Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const UtteranceContinuousParameters& params) = 0;

  // Stop speaking immediately and return true on success.
  virtual bool StopSpeaking() = 0;

  // Returns whether any speech is on going.
  virtual bool IsSpeaking() = 0;

  // Return true if this platform implementation will fire the given event.
  // All platform implementations must fire the TTS_EVENT_END event at a
  // minimum.
  virtual bool SendsEvent(TtsEventType event_type) = 0;

  // Return the gender of the voice, should be either "male" or "female"
  // if known, otherwise the empty string.
  virtual std::string gender();

  virtual std::string error();
  virtual void clear_error();
  virtual void set_error(const std::string& error);

 protected:
  ExtensionTtsPlatformImpl() {}
  virtual ~ExtensionTtsPlatformImpl() {}

  std::string error_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTtsPlatformImpl);
};

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_PLATFORM_H_
