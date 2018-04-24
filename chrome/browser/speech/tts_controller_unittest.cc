// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the TTS Controller.

#include "base/values.h"
#include "chrome/browser/speech/tts_controller_impl.h"
#include "chrome/browser/speech/tts_platform.h"
#include "testing/gtest/include/gtest/gtest.h"

class TtsControllerTest : public testing::Test {
};

// Platform Tts implementation that does nothing.
class DummyTtsPlatformImpl : public TtsPlatformImpl {
 public:
  DummyTtsPlatformImpl() {}
  ~DummyTtsPlatformImpl() override {}
  bool PlatformImplAvailable() override { return true; }
  bool Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params) override {
    return true;
  }
  bool IsSpeaking() override { return false; }
  bool StopSpeaking() override { return true; }
  void Pause() override {}
  void Resume() override {}
  void GetVoices(std::vector<VoiceData>* out_voices) override {}
  std::string error() override { return std::string(); }
  void clear_error() override {}
  void set_error(const std::string& error) override {}
};

// Subclass of TtsController with a public ctor and dtor.
class TestableTtsController : public TtsControllerImpl {
 public:
  TestableTtsController() {}
  ~TestableTtsController() override {}
};

TEST_F(TtsControllerTest, TestTtsControllerShutdown) {
  DummyTtsPlatformImpl platform_impl;
  TestableTtsController* controller =
      new TestableTtsController();
  controller->SetPlatformImpl(&platform_impl);

  Utterance* utterance1 = new Utterance(NULL);
  utterance1->set_can_enqueue(true);
  utterance1->set_src_id(1);
  controller->SpeakOrEnqueue(utterance1);

  Utterance* utterance2 = new Utterance(NULL);
  utterance2->set_can_enqueue(true);
  utterance2->set_src_id(2);
  controller->SpeakOrEnqueue(utterance2);

  // Make sure that deleting the controller when there are pending
  // utterances doesn't cause a crash.
  delete controller;
}

TEST_F(TtsControllerTest, TestGetMatchingVoice) {
  TtsControllerImpl* tts_controller = TtsControllerImpl::GetInstance();

  {
    // Calling GetMatchingVoice with no voices returns -1
    Utterance utterance(nullptr);
    std::vector<VoiceData> voices;
    EXPECT_EQ(-1, tts_controller->GetMatchingVoice(&utterance, voices));
  }

  {
    // Calling GetMatchingVoice with any voices returns the first one
    // even if there are no criteria that match.
    Utterance utterance(nullptr);
    std::vector<VoiceData> voices;
    voices.push_back(VoiceData());
    voices.push_back(VoiceData());
    EXPECT_EQ(0, tts_controller->GetMatchingVoice(&utterance, voices));
  }

  {
    // If nothing else matches, the English voice is returned.
    // (In tests the language will always be English.)
    Utterance utterance(nullptr);
    std::vector<VoiceData> voices;
    VoiceData fr_voice;
    fr_voice.lang = "fr";
    voices.push_back(fr_voice);
    VoiceData en_voice;
    en_voice.lang = "en";
    voices.push_back(en_voice);
    VoiceData de_voice;
    de_voice.lang = "de";
    voices.push_back(de_voice);
    EXPECT_EQ(1, tts_controller->GetMatchingVoice(&utterance, voices));
  }

  {
    // Check precedence of various matching criteria.
    std::vector<VoiceData> voices;
    VoiceData voice0;
    voices.push_back(voice0);
    VoiceData voice1;
    voice1.gender = TTS_GENDER_FEMALE;
    voices.push_back(voice1);
    VoiceData voice2;
    voice2.events.insert(TTS_EVENT_WORD);
    voices.push_back(voice2);
    VoiceData voice3;
    voice3.lang = "de-DE";
    voices.push_back(voice3);
    VoiceData voice4;
    voice4.lang = "fr-CA";
    voices.push_back(voice4);
    VoiceData voice5;
    voice5.name = "Voice5";
    voices.push_back(voice5);
    VoiceData voice6;
    voice6.extension_id = "id6";
    voices.push_back(voice6);

    Utterance utterance(nullptr);
    EXPECT_EQ(0, tts_controller->GetMatchingVoice(&utterance, voices));

    utterance.set_gender(TTS_GENDER_FEMALE);
    EXPECT_EQ(1, tts_controller->GetMatchingVoice(&utterance, voices));

    std::set<TtsEventType> types;
    types.insert(TTS_EVENT_WORD);
    utterance.set_required_event_types(types);
    EXPECT_EQ(2, tts_controller->GetMatchingVoice(&utterance, voices));

    utterance.set_lang("de-DE");
    EXPECT_EQ(3, tts_controller->GetMatchingVoice(&utterance, voices));

    utterance.set_lang("fr-FR");
    EXPECT_EQ(4, tts_controller->GetMatchingVoice(&utterance, voices));

    utterance.set_voice_name("Voice5");
    EXPECT_EQ(5, tts_controller->GetMatchingVoice(&utterance, voices));

    utterance.set_voice_name("");
    utterance.set_extension_id("id6");
    EXPECT_EQ(6, tts_controller->GetMatchingVoice(&utterance, voices));
  }
}
