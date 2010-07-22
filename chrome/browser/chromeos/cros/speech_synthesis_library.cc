// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/speech_synthesis_library.h"

#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "cros/chromeos_speech_synthesis.h"

namespace chromeos {

bool SpeechSynthesisLibraryImpl::Speak(const char* text) {
  return chromeos::Speak(text);
}

bool SpeechSynthesisLibraryImpl::SetSpeakProperties(const char* props) {
  return chromeos::SetSpeakProperties(props);
}

bool SpeechSynthesisLibraryImpl::StopSpeaking() {
  return chromeos::StopSpeaking();
}

bool SpeechSynthesisLibraryImpl::IsSpeaking() {
  return chromeos::IsSpeaking();
}

void SpeechSynthesisLibraryImpl::InitTts(InitStatusCallback callback) {
  chromeos::InitTts(callback);
}

}  // namespace chromeos
