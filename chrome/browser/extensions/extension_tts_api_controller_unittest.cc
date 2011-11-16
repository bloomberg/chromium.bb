// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the TTS API Controller.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/values.h"
#include "chrome/browser/extensions/extension_tts_api_controller.h"
#include "chrome/browser/extensions/extension_tts_api_platform.h"

class ExtensionTtsApiControllerTest : public testing::Test {
};

// Platform Tts implementation that does nothing.
class DummyExtensionTtsPlatformImpl : public ExtensionTtsPlatformImpl {
 public:
  DummyExtensionTtsPlatformImpl() {}
  virtual ~DummyExtensionTtsPlatformImpl() {}
  virtual bool PlatformImplAvailable() { return true; }
  virtual bool Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const UtteranceContinuousParameters& params) {
    return true;
  }
  virtual bool StopSpeaking() { return true; }
  virtual bool SendsEvent(TtsEventType event_type) { return false; }
  virtual std::string gender() { return std::string(); }
  virtual std::string error() { return std::string(); }
  virtual void clear_error() {}
  virtual void set_error(const std::string& error) {}
};

// Subclass of ExtensionTtsController with a public ctor and dtor.
class TestableExtensionTtsController : public ExtensionTtsController {
 public:
  TestableExtensionTtsController() {}
  virtual ~TestableExtensionTtsController() {}
};

TEST_F(ExtensionTtsApiControllerTest, TestTtsControllerShutdown) {
  DummyExtensionTtsPlatformImpl platform_impl;
  TestableExtensionTtsController* controller =
      new TestableExtensionTtsController();
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
