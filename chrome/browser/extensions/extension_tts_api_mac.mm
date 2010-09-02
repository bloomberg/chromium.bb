// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extension_tts_api.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/extensions/extension_function.h"

#import <cocoa/cocoa.h>

static NSSpeechSynthesizer* speech_synthesizer_;

void InitializeSpeechSynthesizer() {
  if (!speech_synthesizer_)
    speech_synthesizer_ = [[NSSpeechSynthesizer alloc] init];
}

bool ExtensionTtsSpeakFunction::RunImpl() {
  InitializeSpeechSynthesizer();
  std::string utterance;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &utterance));
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
