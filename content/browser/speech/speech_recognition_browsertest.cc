// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/speech/google_streaming_remote_engine.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/speech/speech_recognizer_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_input_controller_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::RunLoop;
using content::BrowserThread;
using content::WebContents;

namespace content {

class SpeechRecognitionBrowserTest :
    public ContentBrowserTest,
    public media::TestAudioInputControllerDelegate {
 public:
  // media::TestAudioInputControllerDelegate methods.
  virtual void TestAudioControllerOpened(
      media::TestAudioInputController* controller) OVERRIDE {
    const int capture_packet_interval_ms =
        (1000 * controller->audio_parameters().frames_per_buffer()) /
        controller->audio_parameters().sample_rate();
    ASSERT_EQ(GoogleStreamingRemoteEngine::kAudioPacketIntervalMs,
              capture_packet_interval_ms);
  }

  virtual void TestAudioControllerClosed(
      media::TestAudioInputController* controller) OVERRIDE {
  }

  // Helper methods used by test fixtures.
  GURL GetTestUrlFromFragment(const std::string fragment) {
    return GURL(GetTestUrl("speech", "web_speech_recognition.html").spec() +
        "#" + fragment);
  }

 protected:
  // ContentBrowserTest methods.
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    test_audio_input_controller_factory_.set_delegate(this);
    media::AudioInputController::set_factory_for_testing(
        &test_audio_input_controller_factory_);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(content::SpeechRecognitionManagerImpl::GetInstance());
    content::SpeechRecognizerImpl::SetAudioManagerForTesting(
        new media::MockAudioManager(BrowserThread::GetMessageLoopProxyForThread(
            BrowserThread::IO)));
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    content::SpeechRecognizerImpl::SetAudioManagerForTesting(NULL);
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    test_audio_input_controller_factory_.set_delegate(NULL);
  }

 private:
  media::TestAudioInputControllerFactory test_audio_input_controller_factory_;
};

// Simply loads the test page and checks if it was able to create a Speech
// Recognition object in JavaScript, to make sure the Web Speech API is enabled.
IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest, Precheck) {
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), GetTestUrlFromFragment("precheck"), 2);

  EXPECT_EQ("success", shell()->web_contents()->GetURL().ref());
}

}  // namespace content
