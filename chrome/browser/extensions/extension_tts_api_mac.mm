// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extension_tts_api.h"

#include <string>

#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_function.h"

#import <Cocoa/Cocoa.h>

namespace util = extension_tts_api_util;

class ExtensionTtsPlatformImplMac : public ExtensionTtsPlatformImpl {
 public:
  virtual bool Speak(
      const std::string& utterance,
      const std::string& language,
      const std::string& gender,
      double rate,
      double pitch,
      double volume);

  virtual bool StopSpeaking();

  virtual bool IsSpeaking();

  // Get the single instance of this class.
  static ExtensionTtsPlatformImplMac* GetInstance();

 private:
  ExtensionTtsPlatformImplMac();
  virtual ~ExtensionTtsPlatformImplMac() {}

  NSSpeechSynthesizer* speech_synthesizer_;

  friend struct DefaultSingletonTraits<ExtensionTtsPlatformImplMac>;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTtsPlatformImplMac);
};

// static
ExtensionTtsPlatformImpl* ExtensionTtsPlatformImpl::GetInstance() {
  return ExtensionTtsPlatformImplMac::GetInstance();
}

bool ExtensionTtsPlatformImplMac::Speak(
    const std::string& utterance,
    const std::string& language,
    const std::string& gender,
    double rate,
    double pitch,
    double volume) {
  // NSSpeechSynthesizer equivalents for kGenderKey and kLanguageNameKey do
  // not exist and thus are not supported.

  if (rate >= 0.0) {
    // The TTS api defines rate via words per minute.
    [speech_synthesizer_
        setObject:[NSNumber numberWithInt:rate * 400]
        forProperty:NSSpeechRateProperty error:nil];
  }

  if (pitch >= 0.0) {
    // The TTS api allows an approximate range of 30 to 65 for speech pitch.
    [speech_synthesizer_
        setObject: [NSNumber numberWithInt:(pitch * 35 + 30)]
        forProperty:NSSpeechPitchBaseProperty error:nil];
  }

  if (volume >= 0.0) {
    [speech_synthesizer_
        setObject: [NSNumber numberWithFloat:volume]
        forProperty:NSSpeechVolumeProperty error:nil];
  }

  return [speech_synthesizer_ startSpeakingString:
             [NSString stringWithUTF8String: utterance.c_str()]];
}

bool ExtensionTtsPlatformImplMac::StopSpeaking() {
  [speech_synthesizer_ stopSpeaking];
  return true;
}

bool ExtensionTtsPlatformImplMac::IsSpeaking() {
  return [speech_synthesizer_ isSpeaking];
}

ExtensionTtsPlatformImplMac::ExtensionTtsPlatformImplMac() {
  speech_synthesizer_ = [[NSSpeechSynthesizer alloc] init];
}

// static
ExtensionTtsPlatformImplMac* ExtensionTtsPlatformImplMac::GetInstance() {
  return Singleton<ExtensionTtsPlatformImplMac>::get();
}
