// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_SPEECH_SYNTHESIZER_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_SPEECH_SYNTHESIZER_CLIENT_H_

#include <string>

#include "chromeos/dbus/speech_synthesizer_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockSpeechSynthesizerClient : public SpeechSynthesizerClient {
 public:
  MockSpeechSynthesizerClient();
  virtual ~MockSpeechSynthesizerClient();

  MOCK_METHOD2(Speak, void(const std::string&, const std::string&));
  MOCK_METHOD0(StopSpeaking, void());
  MOCK_METHOD1(IsSpeaking, void(const IsSpeakingCallback&));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_SPEECH_SYNTHESIZER_CLIENT_H_
