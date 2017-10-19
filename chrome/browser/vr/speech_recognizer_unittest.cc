// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/speech_recognizer.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/mock_timer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

static const int kTestSessionId = 1;
const char kTestInterimResult[] = "kitten";
const char kTestResult[] = "cat";

enum FakeRecognitionEvent {
  SPEECH_RECOGNITION_START = 0,
  SPEECH_RECOGNITION_END,
  NETWORK_ERROR,
  SOUND_START,
  SOUND_END,
  AUDIO_START,
  AUDIO_END,
  INTERIM_RESULT,
  FINAL_RESULT,
};

class FakeSpeechRecognitionManager : public content::SpeechRecognitionManager {
 public:
  FakeSpeechRecognitionManager() {}
  ~FakeSpeechRecognitionManager() override {}

  // SpeechRecognitionManager methods.
  int CreateSession(
      const content::SpeechRecognitionSessionConfig& config) override {
    session_ctx_ = config.initial_context;
    session_config_ = config;
    session_id_ = kTestSessionId;
    return session_id_;
  }

  void StartSession(int session_id) override {}

  void AbortSession(int session_id) override {
    DCHECK(session_id_ == session_id);
    session_id_ = 0;
  }

  void AbortAllSessionsForRenderProcess(int render_process_id) override {}
  void AbortAllSessionsForRenderView(int render_process_id,
                                     int render_view_id) override {}
  void StopAudioCaptureForSession(int session_id) override {}

  const content::SpeechRecognitionSessionConfig& GetSessionConfig(
      int session_id) const override {
    DCHECK(session_id_ == session_id);
    return session_config_;
  }

  content::SpeechRecognitionSessionContext GetSessionContext(
      int session_id) const override {
    DCHECK(session_id_ == session_id);
    return session_ctx_;
  }

  int GetSession(int render_process_id,
                 int render_view_id,
                 int request_id) const override {
    return session_id_;
  }

  void FakeSpeechRecognitionEvent(FakeRecognitionEvent event) {
    if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO, FROM_HERE,
          base::BindOnce(
              &FakeSpeechRecognitionManager::FakeSpeechRecognitionEvent,
              base::Unretained(this), event));
      return;
    }
    DCHECK(GetActiveListener());
    content::SpeechRecognitionError error(
        content::SPEECH_RECOGNITION_ERROR_NETWORK);
    switch (event) {
      case SPEECH_RECOGNITION_START:
        GetActiveListener()->OnRecognitionStart(kTestSessionId);
        break;
      case SPEECH_RECOGNITION_END:
        GetActiveListener()->OnRecognitionEnd(kTestSessionId);
        break;
      case NETWORK_ERROR:
        GetActiveListener()->OnRecognitionError(kTestSessionId, error);
        break;
      case SOUND_START:
        GetActiveListener()->OnSoundStart(kTestSessionId);
        break;
      case INTERIM_RESULT:
        SendFakeInterimResults();
        break;
      case FINAL_RESULT:
        SendFakeFinalResults();
        break;
      default:
        NOTREACHED();
    }
  }

  void SendFakeInterimResults() {
    if (!session_id_)
      return;

    content::SpeechRecognitionEventListener* listener = GetActiveListener();
    if (!listener)
      return;
    listener->OnAudioEnd(session_id_);
    content::SpeechRecognitionResult result;
    result.is_provisional = true;
    result.hypotheses.push_back(content::SpeechRecognitionHypothesis(
        base::ASCIIToUTF16(kTestInterimResult), 1.0));
    content::SpeechRecognitionResults results;
    results.push_back(result);
    listener->OnRecognitionResults(session_id_, results);
  }

  void SendFakeFinalResults() {
    if (!session_id_)
      return;

    content::SpeechRecognitionEventListener* listener = GetActiveListener();
    if (!listener)
      return;
    listener->OnAudioEnd(session_id_);
    content::SpeechRecognitionResult result;
    result.hypotheses.push_back(content::SpeechRecognitionHypothesis(
        base::ASCIIToUTF16(kTestResult), 1.0));
    content::SpeechRecognitionResults results;
    results.push_back(result);
    listener->OnRecognitionResults(session_id_, results);
    listener->OnRecognitionEnd(session_id_);
    session_id_ = 0;
  }

 private:
  content::SpeechRecognitionEventListener* GetActiveListener() {
    DCHECK(session_id_ != 0);
    return session_config_.event_listener.get();
  }

  int session_id_ = 0;
  content::SpeechRecognitionSessionContext session_ctx_;
  content::SpeechRecognitionSessionConfig session_config_;

  DISALLOW_COPY_AND_ASSIGN(FakeSpeechRecognitionManager);
};

class SpeechRecognizerTestWrapper : public SpeechRecognizer {
 public:
  SpeechRecognizerTestWrapper() : SpeechRecognizer(nullptr, nullptr, "en") {}

  ~SpeechRecognizerTestWrapper() override {}

  MOCK_METHOD2(OnSpeechResult,
               void(const base::string16& query, bool is_final));
  MOCK_METHOD1(OnSpeechRecognitionStateChanged,
               void(SpeechRecognitionState new_state));

 private:
  DISALLOW_COPY_AND_ASSIGN(SpeechRecognizerTestWrapper);
};

class SpeechRecognizerTest : public testing::Test {
 public:
  SpeechRecognizerTest()
      : fake_speech_recognition_manager_(new FakeSpeechRecognitionManager()),
        speech_recognizer_(new SpeechRecognizerTestWrapper) {
    SpeechRecognizer::SetManagerForTest(fake_speech_recognition_manager_.get());
  }

  ~SpeechRecognizerTest() override {
    SpeechRecognizer::SetManagerForTest(nullptr);
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;
  std::unique_ptr<SpeechRecognizerTestWrapper> speech_recognizer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SpeechRecognizerTest);
};

TEST_F(SpeechRecognizerTest, ReceiveSpeechResult) {
  speech_recognizer_->Start();
  base::RunLoop().RunUntilIdle();

  testing::Sequence s;
  EXPECT_CALL(*speech_recognizer_,
              OnSpeechResult(base::ASCIIToUTF16("kitten"), false))
      .InSequence(s);
  EXPECT_CALL(*speech_recognizer_,
              OnSpeechResult(base::ASCIIToUTF16("cat"), true))
      .InSequence(s);
  // After receiving final speech result, speech recognition should send a reset
  // request.
  EXPECT_CALL(*speech_recognizer_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_READY))
      .InSequence(s);

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(INTERIM_RESULT);
  base::RunLoop().RunUntilIdle();

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(FINAL_RESULT);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SpeechRecognizerTest, ReceivedSpeechRecognitionStates) {
  speech_recognizer_->Start();
  base::RunLoop().RunUntilIdle();

  testing::Sequence s;
  EXPECT_CALL(*speech_recognizer_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_RECOGNIZING))
      .InSequence(s);
  EXPECT_CALL(*speech_recognizer_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_NETWORK_ERROR))
      .InSequence(s);
  EXPECT_CALL(*speech_recognizer_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_READY))
      .InSequence(s);

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(
      SPEECH_RECOGNITION_START);
  base::RunLoop().RunUntilIdle();

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(NETWORK_ERROR);
  base::RunLoop().RunUntilIdle();

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(
      SPEECH_RECOGNITION_END);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SpeechRecognizerTest, NoSoundTimeout) {
  speech_recognizer_->Start();
  base::RunLoop().RunUntilIdle();

  auto mock_timer = std::make_unique<base::MockTimer>(false, false);
  base::MockTimer* timer_ptr = mock_timer.get();
  speech_recognizer_->SetSpeechTimerForTest(std::move(mock_timer));

  testing::Sequence s;
  EXPECT_CALL(*speech_recognizer_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_IN_SPEECH))
      .InSequence(s);
  EXPECT_CALL(*speech_recognizer_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_READY))
      .InSequence(s);

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(SOUND_START);
  base::RunLoop().RunUntilIdle();

  // This should trigger a SPEECH_RECOGNITION_READY state notification.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
}

// This test that it is safe to reset speech_recognizer_ on UI thread after post
// a task to start speech recognition on IO thread.
TEST_F(SpeechRecognizerTest, SafeToResetAfterStart) {
  speech_recognizer_->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*speech_recognizer_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_RECOGNIZING));
  EXPECT_CALL(*speech_recognizer_,
              OnSpeechResult(base::ASCIIToUTF16("cat"), true))
      .Times(0);
  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(
      SPEECH_RECOGNITION_START);
  base::RunLoop().RunUntilIdle();

  fake_speech_recognition_manager_->FakeSpeechRecognitionEvent(FINAL_RESULT);
  // Reset shouldn't crash the test.
  speech_recognizer_.reset(nullptr);
  base::RunLoop().RunUntilIdle();
}

}  // namespace vr
