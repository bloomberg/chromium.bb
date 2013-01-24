// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the TTS Controller.

#include "base/values.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/speech/tts_platform.h"
#include "testing/gtest/include/gtest/gtest.h"

class TtsApiControllerTest : public testing::Test {
};

// Platform Tts implementation that does nothing.
class DummyTtsPlatformImpl : public TtsPlatformImpl {
 public:
  DummyTtsPlatformImpl() {}
  virtual ~DummyTtsPlatformImpl() {}
  virtual bool PlatformImplAvailable() { return true; }
  virtual bool Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const UtteranceContinuousParameters& params) {
    return true;
  }
  virtual bool IsSpeaking() { return false; }
  virtual bool StopSpeaking() { return true; }
  virtual bool SendsEvent(TtsEventType event_type) { return false; }
  virtual std::string gender() { return std::string(); }
  virtual std::string error() { return std::string(); }
  virtual void clear_error() {}
  virtual void set_error(const std::string& error) {}
};

// Subclass of TtsController with a public ctor and dtor.
class TestableTtsController : public TtsController {
 public:
  TestableTtsController() {}
  virtual ~TestableTtsController() {}
};

TEST_F(TtsApiControllerTest, TestTtsControllerShutdown) {
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
