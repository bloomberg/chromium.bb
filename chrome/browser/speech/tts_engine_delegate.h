// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_ENGINE_DELEGATE_H_
#define CHROME_BROWSER_SPEECH_TTS_ENGINE_DELEGATE_H_

#include <vector>

class Utterance;
struct VoiceData;

namespace content {
class BrowserContext;
}

// Interface that delegates TTS requests to user-installed extensions,
// for a given BrowserContext.
class TtsEngineDelegate {
 public:
  virtual ~TtsEngineDelegate();

  // Return a list of all available voices registered.
  virtual void GetVoices(content::BrowserContext* browser_context,
                         std::vector<VoiceData>* out_voices) = 0;

  // Speak the given utterance by sending an event to the given TTS engine.
  virtual void Speak(Utterance* utterance, const VoiceData& voice) = 0;

  // Stop speaking the given utterance by sending an event to the target
  // associated with this utterance.
  virtual void Stop(Utterance* utterance) = 0;

  // Pause in the middle of speaking this utterance.
  virtual void Pause(Utterance* utterance) = 0;

  // Resume speaking this utterance.
  virtual void Resume(Utterance* utterance) = 0;

  // Load the built-in component extension for ChromeOS.
  virtual bool LoadBuiltInTtsExtension() = 0;
};

class TtsEngineDelegateFactory {
 public:
  virtual ~TtsEngineDelegateFactory();

  virtual TtsEngineDelegate* GetTtsEngineDelegateForBrowserContext(
      content::BrowserContext* browser_context) = 0;
};

#endif  // CHROME_BROWSER_SPEECH_TTS_ENGINE_DELEGATE_H__
