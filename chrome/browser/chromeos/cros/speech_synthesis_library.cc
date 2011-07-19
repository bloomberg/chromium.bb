// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/speech_synthesis_library.h"

#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "third_party/cros/chromeos_speech_synthesis.h"

namespace chromeos {

// TODO(chaitanyag): rename to "locale" after making equivalent change in
// Chrome OS code.
const char SpeechSynthesisLibrary::kSpeechPropertyLocale[] = "name";

const char SpeechSynthesisLibrary::kSpeechPropertyGender[] = "gender";
const char SpeechSynthesisLibrary::kSpeechPropertyRate[] = "rate";
const char SpeechSynthesisLibrary::kSpeechPropertyPitch[] = "pitch";
const char SpeechSynthesisLibrary::kSpeechPropertyVolume[] = "volume";
const char SpeechSynthesisLibrary::kSpeechPropertyEquals[] = "=";
const char SpeechSynthesisLibrary::kSpeechPropertyDelimiter[] = ";";

class SpeechSynthesisLibraryImpl : public SpeechSynthesisLibrary {
 public:
  SpeechSynthesisLibraryImpl() {}
  virtual ~SpeechSynthesisLibraryImpl() {}

  bool Speak(const char* text) {
    return chromeos::Speak(text);
  }

  bool SetSpeakProperties(const char* props) {
    return chromeos::SetSpeakProperties(props);
  }

  bool StopSpeaking() {
    return chromeos::StopSpeaking();
  }

  bool IsSpeaking() {
    return chromeos::IsSpeaking();
  }

  void InitTts(InitStatusCallback callback) {
    chromeos::InitTts(callback);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SpeechSynthesisLibraryImpl);
};

class SpeechSynthesisLibraryStubImpl : public SpeechSynthesisLibrary {
 public:
  SpeechSynthesisLibraryStubImpl() {}
  virtual ~SpeechSynthesisLibraryStubImpl() {}
  bool Speak(const char* text) { return true; }
  bool SetSpeakProperties(const char* props) { return true; }
  bool StopSpeaking() { return true; }
  bool IsSpeaking() { return false; }
  void InitTts(InitStatusCallback callback) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SpeechSynthesisLibraryStubImpl);
};

// static
SpeechSynthesisLibrary* SpeechSynthesisLibrary::GetImpl(bool stub) {
  if (stub)
    return new SpeechSynthesisLibraryStubImpl();
  else
    return new SpeechSynthesisLibraryImpl();
}

}  // namespace chromeos
