// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_H_

#include <vector>

#include "base/memory/singleton.h"
#include "chrome/browser/speech/tts_controller.h"
#include "extensions/browser/extension_function.h"

class Utterance;

namespace base {
class ListValue;
}

namespace extensions {
class Extension;
}

namespace tts_engine_events {
extern const char kOnSpeak[];
extern const char kOnStop[];
extern const char kOnPause[];
extern const char kOnResume[];
}

// TtsEngineDelegate implementation used by TtsController.
class TtsExtensionEngine : public TtsEngineDelegate {
 public:
  static TtsExtensionEngine* GetInstance();

  // Overridden from TtsEngineDelegate:
  virtual void GetVoices(Profile* profile,
                 std::vector<VoiceData>* out_voices) OVERRIDE;
  virtual void Speak(Utterance* utterance, const VoiceData& voice) OVERRIDE;
  virtual void Stop(Utterance* utterance) OVERRIDE;
  virtual void Pause(Utterance* utterance) OVERRIDE;
  virtual void Resume(Utterance* utterance) OVERRIDE;
};

// Hidden/internal extension function used to allow TTS engine extensions
// to send events back to the client that's calling tts.speak().
class ExtensionTtsEngineSendTtsEventFunction : public SyncExtensionFunction {
 private:
  virtual ~ExtensionTtsEngineSendTtsEventFunction() {}
  virtual bool RunSync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("ttsEngine.sendTtsEvent", TTSENGINE_SENDTTSEVENT)
};

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_H_
