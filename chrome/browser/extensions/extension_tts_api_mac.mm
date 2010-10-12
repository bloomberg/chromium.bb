// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extension_tts_api.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/extensions/extension_function.h"

#import <Cocoa/Cocoa.h>

namespace util = extension_tts_api_util;

static NSSpeechSynthesizer* speech_synthesizer_;

void InitializeSpeechSynthesizer() {
  if (!speech_synthesizer_)
    speech_synthesizer_ = [[NSSpeechSynthesizer alloc] init];
}

bool ExtensionTtsSpeakFunction::RunImpl() {
  InitializeSpeechSynthesizer();
  std::string utterance;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &utterance));
  DictionaryValue* speak_options = NULL;

  // Parse speech properties.
  if (args_->GetDictionary(1, &speak_options)) {
    std::string str_value;
    double real_value;
    // NSSpeechSynthesizer equivalents for kGenderKey and kLanguageNameKey do
    // not exist and thus are not supported.
    if (util::ReadNumberByKey(speak_options, util::kRateKey, &real_value)) {
      // The TTS api defines rate via words per minute.
      [speech_synthesizer_
          setObject:[NSNumber numberWithInt:real_value*400]
          forProperty:NSSpeechRateProperty error:nil];
    }
    if (util::ReadNumberByKey(speak_options, util::kPitchKey, &real_value)) {
      // The TTS api allows an approximate range of 30 to 65 for speech pitch.
      [speech_synthesizer_
          setObject: [NSNumber numberWithInt:(real_value*35 + 30)]
          forProperty:NSSpeechPitchBaseProperty error:nil];
    }
    if (util::ReadNumberByKey(speak_options, util::kVolumeKey, &real_value)) {
      // The TTS api allows a range of 0.0 to 1.0 for speech volume.
      [speech_synthesizer_
          setObject: [NSNumber numberWithFloat:real_value]
          forProperty:NSSpeechVolumeProperty error:nil];
    }
  }

  return
      [speech_synthesizer_ startSpeakingString:
           [NSString stringWithUTF8String: utterance.c_str()]];
}

bool ExtensionTtsStopSpeakingFunction::RunImpl() {
    InitializeSpeechSynthesizer();
  [speech_synthesizer_ stopSpeaking];
  return true;
}

bool ExtensionTtsIsSpeakingFunction::RunImpl() {
    InitializeSpeechSynthesizer();
  return [speech_synthesizer_ isSpeaking];
}
