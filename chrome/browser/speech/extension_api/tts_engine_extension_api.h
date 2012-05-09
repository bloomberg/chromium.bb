// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_H_
#pragma once

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_function.h"

class Extension;
class Utterance;

namespace base {
class ListValue;
}

// Return a list of all available voices registered by extensions.
void GetExtensionVoices(Profile* profile, base::ListValue* result_voices);

// Find the first extension with a tts_voices in its
// manifest that matches the speech parameters of this utterance.
// If found, store a pointer to the extension in |matching_extension| and
// the index of the voice within the extension in |voice_index| and
// return true.
bool GetMatchingExtensionVoice(Utterance* utterance,
                               const Extension** matching_extension,
                               size_t* voice_index);

// Speak the given utterance by sending an event to the given TTS engine
// extension voice.
void ExtensionTtsEngineSpeak(Utterance* utterance,
                             const Extension* extension,
                             size_t voice_index);

// Stop speaking the given utterance by sending an event to the extension
// associated with this utterance.
void ExtensionTtsEngineStop(Utterance* utterance);

// Hidden/internal extension function used to allow TTS engine extensions
// to send events back to the client that's calling tts.speak().
class ExtensionTtsEngineSendTtsEventFunction : public SyncExtensionFunction {
 private:
  virtual ~ExtensionTtsEngineSendTtsEventFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("ttsEngine.sendTtsEvent")
};

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_H_
