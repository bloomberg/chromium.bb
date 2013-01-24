// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/speech/extension_api/tts_extension_api.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/speech/tts_platform.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Needed for CreateFunctor.
#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

using ::testing::AnyNumber;
using ::testing::CreateFunctor;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

class MockTtsPlatformImpl : public TtsPlatformImpl {
 public:
  MockTtsPlatformImpl()
      : ALLOW_THIS_IN_INITIALIZER_LIST(ptr_factory_(this)) {}

  virtual bool PlatformImplAvailable() {
    return true;
  }

  virtual bool SendsEvent(TtsEventType event_type) {
    return (event_type == TTS_EVENT_END ||
            event_type == TTS_EVENT_WORD);
  }


  MOCK_METHOD4(Speak,
               bool(int utterance_id,
                    const std::string& utterance,
                    const std::string& lang,
                    const UtteranceContinuousParameters& params));
  MOCK_METHOD0(StopSpeaking, bool(void));

  MOCK_METHOD0(IsSpeaking, bool(void));

  void SetErrorToEpicFail() {
    set_error("epic fail");
  }

  void SendEndEvent(int utterance_id,
                    const std::string& utterance,
                    const std::string& lang,
                    const UtteranceContinuousParameters& params) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(
            &MockTtsPlatformImpl::SendEvent,
            ptr_factory_.GetWeakPtr(),
            false, utterance_id, TTS_EVENT_END, utterance.size(),
            std::string()),
        base::TimeDelta());
  }

  void SendEndEventWhenQueueNotEmpty(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const UtteranceContinuousParameters& params) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(
            &MockTtsPlatformImpl::SendEvent,
            ptr_factory_.GetWeakPtr(),
            true, utterance_id, TTS_EVENT_END, utterance.size(), std::string()),
        base::TimeDelta());
  }

  void SendWordEvents(int utterance_id,
                      const std::string& utterance,
                      const std::string& lang,
                      const UtteranceContinuousParameters& params) {
    for (int i = 0; i < static_cast<int>(utterance.size()); i++) {
      if (i == 0 || utterance[i - 1] == ' ') {
        MessageLoop::current()->PostDelayedTask(
            FROM_HERE, base::Bind(
                &MockTtsPlatformImpl::SendEvent,
                ptr_factory_.GetWeakPtr(),
                false, utterance_id, TTS_EVENT_WORD, i,
                std::string()),
            base::TimeDelta());
      }
    }
  }

  void SendEvent(bool wait_for_non_empty_queue,
                 int utterance_id,
                 TtsEventType event_type,
                 int char_index,
                 const std::string& message) {
    TtsController* controller = TtsController::GetInstance();
    if (wait_for_non_empty_queue && controller->QueueSize() == 0) {
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE, base::Bind(
              &MockTtsPlatformImpl::SendEvent,
              ptr_factory_.GetWeakPtr(),
              true, utterance_id, event_type, char_index, message),
          base::TimeDelta::FromMilliseconds(100));
      return;
    }

    controller->OnTtsEvent(utterance_id, event_type, char_index, message);
  }

 private:
  base::WeakPtrFactory<MockTtsPlatformImpl> ptr_factory_;
};

class TtsApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpInProcessBrowserTestFixture() {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    TtsController::GetInstance()->SetPlatformImpl(&mock_platform_impl_);
  }

 protected:
  StrictMock<MockTtsPlatformImpl> mock_platform_impl_;
};

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakOptionalArgs) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());

  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "", _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "Alpha", _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "Bravo", _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "Charlie", _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "Echo", _, _))
      .WillOnce(Return(true));
  ASSERT_TRUE(RunExtensionTest("tts/optional_args")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakFinishesImmediately) {
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
  ASSERT_TRUE(RunExtensionTest("tts/speak_once")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakInterrupt) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());

  // One utterance starts speaking, and then a second interrupts.
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "text 1", _, _))
      .WillOnce(Return(true));
  // Expect the second utterance and allow it to finish.
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "text 2", _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
  ASSERT_TRUE(RunExtensionTest("tts/interrupt")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakQueueInterrupt) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());

  // In this test, two utterances are queued, and then a third
  // interrupts. Speak() never gets called on the second utterance.
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "text 1", _, _))
      .WillOnce(Return(true));
  // Don't expect the second utterance, because it's queued up and the
  // first never finishes.
  // Expect the third utterance and allow it to finish successfully.
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "text 3", _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
  ASSERT_TRUE(RunExtensionTest("tts/queue_interrupt")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakEnqueue) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());

  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "text 1", _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEventWhenQueueNotEmpty),
          Return(true)));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "text 2", _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
  ASSERT_TRUE(RunExtensionTest("tts/enqueue")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakError) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .Times(AnyNumber());

  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "first try", _, _))
      .WillOnce(DoAll(
          InvokeWithoutArgs(
              CreateFunctor(&mock_platform_impl_,
                            &MockTtsPlatformImpl::SetErrorToEpicFail)),
          Return(false)));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "second try", _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
  ASSERT_TRUE(RunExtensionTest("tts/speak_error")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformWordCallbacks) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());

  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "one two three", _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendWordEvents),
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
  ASSERT_TRUE(RunExtensionTest("tts/word_callbacks")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, RegisterEngine) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .Times(AnyNumber());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillRepeatedly(Return(true));

  {
    InSequence s;
    EXPECT_CALL(mock_platform_impl_, Speak(_, "native speech", _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
    EXPECT_CALL(mock_platform_impl_, Speak(_, "native speech 2", _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
    EXPECT_CALL(mock_platform_impl_, Speak(_, "native speech 3", _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
  }

  ASSERT_TRUE(RunExtensionTest("tts_engine/register_engine")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, EngineError) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(RunExtensionTest("tts_engine/engine_error")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, EngineWordCallbacks) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(RunExtensionTest("tts_engine/engine_word_callbacks")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, LangMatching) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(RunExtensionTest("tts_engine/lang_matching")) << message_;
}

// http://crbug.com/122474
IN_PROC_BROWSER_TEST_F(TtsApiTest, EngineApi) {
  ASSERT_TRUE(RunExtensionTest("tts_engine/engine_api")) << message_;
}
