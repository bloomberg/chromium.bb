// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_INPUT_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_INPUT_DISPATCHER_HOST_H_

#include "base/scoped_ptr.h"
#include "content/browser/browser_message_filter.h"
#include "content/browser/speech/speech_input_manager.h"

struct SpeechInputHostMsg_StartRecognition_Params;

namespace speech_input {

// SpeechInputDispatcherHost is a delegate for Speech API messages used by
// RenderMessageFilter.
// It's the complement of SpeechInputDispatcher (owned by RenderView).
class SpeechInputDispatcherHost : public BrowserMessageFilter,
                                  public SpeechInputManager::Delegate {
 public:
  class SpeechInputCallers;

  explicit SpeechInputDispatcherHost(int render_process_id);

  // SpeechInputManager::Delegate methods.
  virtual void SetRecognitionResult(int caller_id,
                                    const SpeechInputResultArray& result);
  virtual void DidCompleteRecording(int caller_id);
  virtual void DidCompleteRecognition(int caller_id);

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

  // Singleton accessor setter useful for tests.
  static void set_manager_accessor(SpeechInputManager::AccessorMethod* method) {
    manager_accessor_ = method;
  }

 private:
  virtual ~SpeechInputDispatcherHost();

  void OnStartRecognition(
      const SpeechInputHostMsg_StartRecognition_Params &params);
  void OnCancelRecognition(int render_view_id, int request_id);
  void OnStopRecording(int render_view_id, int request_id);

  // Returns the speech input manager to forward events to, creating one if
  // needed.
  SpeechInputManager* manager();

  int render_process_id_;
  bool may_have_pending_requests_;  // Set if we received any speech IPC request

  static SpeechInputManager::AccessorMethod* manager_accessor_;

  DISALLOW_COPY_AND_ASSIGN(SpeechInputDispatcherHost);
};

}  // namespace speech_input

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_INPUT_DISPATCHER_HOST_H_
