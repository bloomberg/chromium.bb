// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FAKE_SPEECH_RECOGNTION_MANAGER_H_
#define CONTENT_PUBLIC_TEST_FAKE_SPEECH_RECOGNTION_MANAGER_H_

#include "base/callback_forward.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"

namespace content {

// Fake SpeechRecognitionManager class that can be used for tests.
// By default the recognition manager will respond with "Pictures of the moon"
// as recognized result from speech. This result can be overridden by calling
// SetFakeResult().
class FakeSpeechRecognitionManager : public SpeechRecognitionManager {
 public:
  FakeSpeechRecognitionManager();
  virtual ~FakeSpeechRecognitionManager();

  const std::string& grammar() const {
    return grammar_;
  }

  bool did_cancel_all() {
    return did_cancel_all_;
  }

  void set_should_send_fake_response(bool send) {
    should_send_fake_response_ = send;
  }

  bool should_send_fake_response() {
    return should_send_fake_response_;
  }

  void WaitForRecognitionStarted();

  void SetFakeResult(const std::string& result);

  // SpeechRecognitionManager methods.
  virtual int CreateSession(
      const SpeechRecognitionSessionConfig& config) OVERRIDE;
  virtual void StartSession(int session_id) OVERRIDE;
  virtual void AbortSession(int session_id) OVERRIDE;
  virtual void StopAudioCaptureForSession(int session_id) OVERRIDE;
  virtual void AbortAllSessionsForRenderProcess(int render_process_id) OVERRIDE;
  virtual void AbortAllSessionsForRenderView(int render_process_id,
                                             int render_view_id) OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual base::string16 GetAudioInputDeviceModel() OVERRIDE;
  virtual void ShowAudioInputSettings() OVERRIDE {}
  virtual int GetSession(int render_process_id,
                         int render_view_id,
                         int request_id) const OVERRIDE;
  virtual const SpeechRecognitionSessionConfig& GetSessionConfig(
      int session_id) const OVERRIDE;
  virtual SpeechRecognitionSessionContext GetSessionContext(
      int session_id) const OVERRIDE;

 private:
  void SetFakeRecognitionResult();

  int session_id_;
  SpeechRecognitionEventListener* listener_;
  SpeechRecognitionSessionConfig session_config_;
  SpeechRecognitionSessionContext session_ctx_;
  std::string fake_result_;
  std::string grammar_;
  bool did_cancel_all_;
  bool should_send_fake_response_;
  base::Closure recognition_started_closure_;

  DISALLOW_COPY_AND_ASSIGN(FakeSpeechRecognitionManager);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_SPEECH_RECOGNTION_MANAGER_H_
