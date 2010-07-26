// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_SPEECH_SYNTHESIS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_SPEECH_SYNTHESIS_LIBRARY_H_
#pragma once

#include "chrome/browser/chromeos/cros/speech_synthesis_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockSpeechSynthesisLibrary : public SpeechSynthesisLibrary {
 public:
  MockSpeechSynthesisLibrary() {}
  virtual ~MockSpeechSynthesisLibrary() {}
  MOCK_METHOD1(Speak, bool(const char*));
  MOCK_METHOD1(SetSpeakProperties, bool(const char*));
  MOCK_METHOD0(StopSpeaking, bool(void));
  MOCK_METHOD0(IsSpeaking, bool(void));
  MOCK_METHOD1(InitTts, void(InitStatusCallback));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_SPEECH_SYNTHESIS_LIBRARY_H_
