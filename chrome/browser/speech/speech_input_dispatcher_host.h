// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_INPUT_DISPATCHER_HOST_H_
#define CHROME_BROWSER_SPEECH_SPEECH_INPUT_DISPATCHER_HOST_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/speech/speech_input_manager.h"
#include "ipc/ipc_message.h"

namespace speech_input {

// SpeechInputDispatcherHost is a delegate for Speech API messages used by
// ResourceMessageFilter.
// It's the complement of SpeechInputDispatcher (owned by RenderView).
class SpeechInputDispatcherHost
    : public base::RefCountedThreadSafe<SpeechInputDispatcherHost>,
      public SpeechInputManager::Delegate {
 public:
  explicit SpeechInputDispatcherHost(int resource_message_filter_process_id);

  // SpeechInputManager::Delegate methods.
  void SetRecognitionResult(int caller_id,
                            const SpeechInputResultArray& result);
  void DidCompleteRecording(int caller_id);
  void DidCompleteRecognition(int caller_id);

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled.
  bool OnMessageReceived(const IPC::Message& msg, bool* msg_was_ok);

  // Singleton accessor setter useful for tests.
  static void set_manager_accessor(SpeechInputManager::AccessorMethod* method) {
    manager_accessor_ = method;
  }

 private:
  class SpeechInputCallers;
  friend class base::RefCountedThreadSafe<SpeechInputDispatcherHost>;

  virtual ~SpeechInputDispatcherHost();
  void SendMessageToRenderView(IPC::Message* message, int render_view_id);

  void OnStartRecognition(int render_view_id, int request_id,
                          const gfx::Rect& element_rect,
                          const std::string& language,
                          const std::string& grammar);
  void OnCancelRecognition(int render_view_id, int request_id);
  void OnStopRecording(int render_view_id, int request_id);

  // Returns the speech input manager to forward events to, creating one if
  // needed.
  SpeechInputManager* manager();

  int resource_message_filter_process_id_;
  SpeechInputCallers* callers_;  // weak reference to a singleton.

  static SpeechInputManager::AccessorMethod* manager_accessor_;

  DISALLOW_COPY_AND_ASSIGN(SpeechInputDispatcherHost);
};

}  // namespace speech_input

#endif  // CHROME_BROWSER_SPEECH_SPEECH_INPUT_DISPATCHER_HOST_H_
