// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_speech_recognition_manager.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/common/speech_recognition_result.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestResult[] = "Pictures of the moon";

void RunCallback(const base::Closure recognition_started_closure) {
  recognition_started_closure.Run();
}
}  // namespace

namespace content {

FakeSpeechRecognitionManager::FakeSpeechRecognitionManager()
    : session_id_(0),
      listener_(NULL),
      fake_result_(kTestResult),
      did_cancel_all_(false),
      should_send_fake_response_(true) {
}

FakeSpeechRecognitionManager::~FakeSpeechRecognitionManager() {
}

void FakeSpeechRecognitionManager::WaitForRecognitionStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
  recognition_started_closure_ = runner->QuitClosure();
  runner->Run();
  recognition_started_closure_.Reset();
}

void FakeSpeechRecognitionManager::SetFakeResult(const std::string& value) {
  fake_result_ = value;
}

int FakeSpeechRecognitionManager::CreateSession(
    const SpeechRecognitionSessionConfig& config) {
  VLOG(1) << "FAKE CreateSession invoked.";
  EXPECT_EQ(0, session_id_);
  EXPECT_EQ(NULL, listener_);
  listener_ = config.event_listener.get();
  if (config.grammars.size() > 0)
    grammar_ = config.grammars[0].url;
  session_ctx_ = config.initial_context;
  session_config_ = config;
  session_id_ = 1;
  return session_id_;
}

void FakeSpeechRecognitionManager::StartSession(int session_id) {
  VLOG(1) << "FAKE StartSession invoked.";
  EXPECT_EQ(session_id, session_id_);
  EXPECT_TRUE(listener_ != NULL);

  if (should_send_fake_response_) {
    // Give the fake result in a short while.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            &FakeSpeechRecognitionManager::SetFakeRecognitionResult,
            // This class does not need to be refcounted (typically done by
            // PostTask) since it will outlive the test and gets released only
            // when the test shuts down. Disabling refcounting here saves a bit
            // of unnecessary code and the factory method can return a plain
            // pointer below as required by the real code.
            base::Unretained(this)));
  }
  if (!recognition_started_closure_.is_null()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&RunCallback, recognition_started_closure_));
  }
}

void FakeSpeechRecognitionManager::AbortSession(int session_id) {
  VLOG(1) << "FAKE AbortSession invoked.";
  EXPECT_EQ(session_id_, session_id);
  session_id_ = 0;
  listener_ = NULL;
}

void FakeSpeechRecognitionManager::StopAudioCaptureForSession(int session_id) {
  VLOG(1) << "StopRecording invoked.";
  EXPECT_EQ(session_id_, session_id);
  // Nothing to do here since we aren't really recording.
}

void FakeSpeechRecognitionManager::AbortAllSessionsForRenderProcess(
    int render_process_id) {
  VLOG(1) << "CancelAllRequestsWithDelegate invoked.";
  EXPECT_TRUE(should_send_fake_response_ ||
              session_ctx_.render_process_id == render_process_id);
  did_cancel_all_ = true;
}

void FakeSpeechRecognitionManager::AbortAllSessionsForRenderView(
    int render_process_id, int render_view_id) {
  NOTREACHED();
}

bool FakeSpeechRecognitionManager::HasAudioInputDevices() { return true; }

base::string16 FakeSpeechRecognitionManager::GetAudioInputDeviceModel() {
  return base::string16();
}

int FakeSpeechRecognitionManager::GetSession(int render_process_id,
                                             int render_view_id,
                                             int request_id) const {
  return session_ctx_.render_process_id == render_process_id &&
         session_ctx_.render_view_id == render_view_id &&
         session_ctx_.request_id == request_id;
}

const SpeechRecognitionSessionConfig&
    FakeSpeechRecognitionManager::GetSessionConfig(int session_id) const {
  EXPECT_EQ(session_id, session_id_);
  return session_config_;
}

SpeechRecognitionSessionContext FakeSpeechRecognitionManager::GetSessionContext(
    int session_id) const {
  EXPECT_EQ(session_id, session_id_);
  return session_ctx_;
}

void FakeSpeechRecognitionManager::SetFakeRecognitionResult() {
  if (!session_id_)  // Do a check in case we were cancelled..
    return;

  VLOG(1) << "Setting fake recognition result.";
  listener_->OnAudioEnd(session_id_);
  SpeechRecognitionResult result;
  result.hypotheses.push_back(SpeechRecognitionHypothesis(
      base::ASCIIToUTF16(kTestResult), 1.0));
  SpeechRecognitionResults results;
  results.push_back(result);
  listener_->OnRecognitionResults(session_id_, results);
  listener_->OnRecognitionEnd(session_id_);
  session_id_ = 0;
  listener_ = NULL;
  VLOG(1) << "Finished setting fake recognition result.";
}

}  // namespace content
