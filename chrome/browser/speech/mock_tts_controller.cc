// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/singleton.h"
#include "chrome/browser/speech/tts_controller.h"

UtteranceContinuousParameters::UtteranceContinuousParameters()
    : rate(1.0), pitch(1.0), volume(1.0) {}

VoiceData::VoiceData() : remote(false), native(false) {}

VoiceData::VoiceData(const VoiceData& other) = default;

VoiceData::~VoiceData() {}

class MockTtsController : public TtsController {
 public:
  static MockTtsController* GetInstance() {
    return base::Singleton<MockTtsController>::get();
  }

  MockTtsController() {}

  bool IsSpeaking() override { return false; }

  void SpeakOrEnqueue(Utterance* utterance) override {}

  void Stop() override {}

  void Pause() override {}

  void Resume() override {}

  void OnTtsEvent(int utterance_id,
                  TtsEventType event_type,
                  int char_index,
                  const std::string& error_message) override {}

  void GetVoices(content::BrowserContext* browser_context,
                 std::vector<VoiceData>* out_voices) override {}

  void VoicesChanged() override {}

  void AddVoicesChangedDelegate(VoicesChangedDelegate* delegate) override {}

  void RemoveVoicesChangedDelegate(VoicesChangedDelegate* delegate) override {}

  void RemoveUtteranceEventDelegate(UtteranceEventDelegate* delegate) override {
  }

  void SetTtsEngineDelegate(TtsEngineDelegate* delegate) override {}

  TtsEngineDelegate* GetTtsEngineDelegate() override { return nullptr; }

  void SetPlatformImpl(TtsPlatformImpl* platform_impl) override {}

  int QueueSize() override { return 0; }

 private:
  friend struct base::DefaultSingletonTraits<MockTtsController>;
  DISALLOW_COPY_AND_ASSIGN(MockTtsController);
};

// static
TtsController* TtsController::GetInstance() {
  return MockTtsController::GetInstance();
}
