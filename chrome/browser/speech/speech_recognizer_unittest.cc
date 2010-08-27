// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/speech/speech_recognizer.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "media/audio/test_audio_input_controller_factory.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::AudioInputController;
using media::TestAudioInputController;
using media::TestAudioInputControllerFactory;

namespace {
const int kAudioPacketLengthBytes = 1000;
}

namespace speech_input {

class SpeechRecognizerTest : public SpeechRecognizerDelegate,
                             public testing::Test {
 public:
  SpeechRecognizerTest()
      : io_thread_(ChromeThread::IO, &message_loop_),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            recognizer_(new SpeechRecognizer(this, 1))),
        recording_complete_(false),
        recognition_complete_(false),
        result_received_(false),
        error_(false) {
  }

  void StartTest() {
    EXPECT_TRUE(recognizer_->StartRecording());
  }

  // SpeechRecognizer::Delegate methods.
  virtual void SetRecognitionResult(int caller_id,
                                    bool error,
                                    const string16& result) {
    result_received_ = true;
  }

  virtual void DidCompleteRecording(int caller_id) {
    recording_complete_ = true;
  }

  virtual void DidCompleteRecognition(int caller_id) {
    recognition_complete_ = true;
  }

  virtual void OnRecognizerError(int caller_id) {
    error_ = true;
  }

  // testing::Test methods.
  virtual void SetUp() {
    URLFetcher::set_factory(&url_fetcher_factory_);
    AudioInputController::set_factory(&audio_input_controller_factory_);
  }

  virtual void TearDown() {
    URLFetcher::set_factory(NULL);
    AudioInputController::set_factory(NULL);
  }

 protected:
  MessageLoopForIO message_loop_;
  ChromeThread io_thread_;
  scoped_refptr<SpeechRecognizer> recognizer_;
  bool recording_complete_;
  bool recognition_complete_;
  bool result_received_;
  bool error_;
  TestURLFetcherFactory url_fetcher_factory_;
  TestAudioInputControllerFactory audio_input_controller_factory_;
};

TEST_F(SpeechRecognizerTest, StopNoData) {
  // Check for callbacks when stopping record before any audio gets recorded.
  EXPECT_TRUE(recognizer_->StartRecording());
  recognizer_->CancelRecognition();
  EXPECT_FALSE(recording_complete_);
  EXPECT_FALSE(recognition_complete_);
  EXPECT_FALSE(result_received_);
  EXPECT_FALSE(error_);
}

TEST_F(SpeechRecognizerTest, CancelNoData) {
  // Check for callbacks when canceling recognition before any audio gets
  // recorded.
  EXPECT_TRUE(recognizer_->StartRecording());
  recognizer_->StopRecording();
  EXPECT_TRUE(recording_complete_);
  EXPECT_TRUE(recognition_complete_);
  EXPECT_FALSE(result_received_);
  EXPECT_FALSE(error_);
}

TEST_F(SpeechRecognizerTest, StopWithData) {
  uint8 data[kAudioPacketLengthBytes] = { 0 };

  // Start recording, give some data and then stop. This should wait for the
  // network callback to arrive before completion.
  EXPECT_TRUE(recognizer_->StartRecording());
  TestAudioInputController* controller =
      audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);
  controller = audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);
  controller->event_handler()->OnData(controller, data, sizeof(data));
  MessageLoop::current()->RunAllPending();
  recognizer_->StopRecording();
  EXPECT_TRUE(recording_complete_);
  EXPECT_FALSE(recognition_complete_);
  EXPECT_FALSE(result_received_);
  EXPECT_FALSE(error_);

  // Issue the network callback to complete the process.
  TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  URLRequestStatus status;
  status.set_status(URLRequestStatus::SUCCESS);
  fetcher->delegate()->OnURLFetchComplete(fetcher, fetcher->original_url(),
                                          status, 200, ResponseCookies(), "");
  EXPECT_TRUE(recognition_complete_);
  EXPECT_TRUE(result_received_);
  EXPECT_FALSE(error_);
}

TEST_F(SpeechRecognizerTest, CancelWithData) {
  uint8 data[kAudioPacketLengthBytes] = { 0 };

  // Start recording, give some data and then cancel. This should not create
  // a network request and finish immediately.
  EXPECT_TRUE(recognizer_->StartRecording());
  TestAudioInputController* controller =
      audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);
  controller->event_handler()->OnData(controller, data, sizeof(data));
  MessageLoop::current()->RunAllPending();
  recognizer_->CancelRecognition();
  EXPECT_EQ(NULL, url_fetcher_factory_.GetFetcherByID(0));
  EXPECT_FALSE(recording_complete_);
  EXPECT_FALSE(recognition_complete_);
  EXPECT_FALSE(result_received_);
  EXPECT_FALSE(error_);
}

TEST_F(SpeechRecognizerTest, AudioControllerErrorNoData) {
  // Check if things tear down properly if AudioInputController threw an error.
  EXPECT_TRUE(recognizer_->StartRecording());
  TestAudioInputController* controller =
      audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);
  controller->event_handler()->OnError(controller, 0);
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(recording_complete_);
  EXPECT_TRUE(recognition_complete_);
  EXPECT_FALSE(result_received_);
  EXPECT_TRUE(error_);
}

TEST_F(SpeechRecognizerTest, AudioControllerErrorWithData) {
  uint8 data[kAudioPacketLengthBytes] = { 0 };

  // Check if things tear down properly if AudioInputController threw an error
  // after giving some audio data.
  EXPECT_TRUE(recognizer_->StartRecording());
  TestAudioInputController* controller =
      audio_input_controller_factory_.controller();
  ASSERT_TRUE(controller);
  controller->event_handler()->OnData(controller, data, sizeof(data));
  controller->event_handler()->OnError(controller, 0);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(NULL, url_fetcher_factory_.GetFetcherByID(0));
  EXPECT_TRUE(recording_complete_);
  EXPECT_TRUE(recognition_complete_);
  EXPECT_FALSE(result_received_);
  EXPECT_TRUE(error_);
}

}  // namespace speech_input
