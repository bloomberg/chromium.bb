// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tts_api.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Needed for CreateFunctor.
#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_mock.h"
#endif

using ::testing::AnyNumber;
using ::testing::CreateFunctor;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

class MockExtensionTtsPlatformImpl : public ExtensionTtsPlatformImpl {
 public:
  MOCK_METHOD6(Speak,
               bool(const std::string& utterance,
                    const std::string& locale,
                    const std::string& gender,
                    double rate,
                    double pitch,
                    double volume));
  MOCK_METHOD0(StopSpeaking, bool(void));
  MOCK_METHOD0(IsSpeaking, bool(void));

  void SetErrorToEpicFail() {
    set_error("epic fail");
  }
};

class TtsApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    ExtensionTtsController::GetInstance()->SetPlatformImpl(
        &mock_platform_impl_);
  }

 protected:
  StrictMock<MockExtensionTtsPlatformImpl> mock_platform_impl_;
};

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakFinishesImmediately) {
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, _, _, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .WillOnce(Return(false));
  ASSERT_TRUE(RunExtensionTest("tts/speak_once")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakKeepsSpeakingTwice) {
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, _, _, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  ASSERT_TRUE(RunExtensionTest("tts/speak_once")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakInterrupt) {
  // One utterances starts speaking, and then a second interrupts.
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak("text 1", _, _, _, _, _))
      .WillOnce(Return(true));

  // Ensure that the first utterance keeps going until it's interrupted.
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));

  // Expect the second utterance and allow it to continue for two calls to
  // IsSpeaking and then finish successfully.
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak("text 2", _, _, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  ASSERT_TRUE(RunExtensionTest("tts/interrupt")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakQueueInterrupt) {
  // In this test, two utterances are queued, and then a third
  // interrupts. Speak() never gets called on the second utterance.
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak("text 1", _, _, _, _, _))
      .WillOnce(Return(true));

  // Ensure that the first utterance keeps going until it's interrupted.
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));

  // Expect the third utterance and allow it to continue for two calls to
  // IsSpeaking and then finish successfully.
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak("text 3", _, _, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  ASSERT_TRUE(RunExtensionTest("tts/queue_interrupt")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakEnqueue) {
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak("text 1", _, _, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_platform_impl_, Speak("text 2", _, _, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  ASSERT_TRUE(RunExtensionTest("tts/enqueue")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakError) {
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, _, _, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .WillOnce(Return(false));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, _, _, _, _, _))
      .WillOnce(DoAll(
          InvokeWithoutArgs(
              CreateFunctor(&mock_platform_impl_,
                            &MockExtensionTtsPlatformImpl::SetErrorToEpicFail)),
          Return(false)));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, _, _, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .WillOnce(Return(false));
  ASSERT_TRUE(RunExtensionTest("tts/speak_error")) << message_;
}

#if defined(OS_WIN)
// Flakily fails on Windows: http://crbug.com/70198
#define MAYBE_Provide FLAKY_Provide
#else
#define MAYBE_Provide Provide
#endif
IN_PROC_BROWSER_TEST_F(TtsApiTest, MAYBE_Provide) {
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .WillRepeatedly(Return(false));

  {
    InSequence s;
    EXPECT_CALL(mock_platform_impl_, Speak("native speech", _, _, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_platform_impl_, Speak("native speech 2", _, _, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_platform_impl_, Speak("native speech 3", _, _, _, _, _))
        .WillOnce(Return(true));
  }

  ASSERT_TRUE(RunExtensionTest("tts/provide")) << message_;
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TtsChromeOs) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  chromeos::CrosMock crosMock;
  crosMock.InitMockSpeechSynthesisLibrary();
  crosMock.SetSpeechSynthesisLibraryExpectations();

  ASSERT_TRUE(RunExtensionTest("tts/chromeos")) << message_;
}
#endif
