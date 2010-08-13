// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_INPUT_DISPATCHER_HOST_H_
#define CHROME_BROWSER_SPEECH_SPEECH_INPUT_DISPATCHER_HOST_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "ipc/ipc_message.h"
#include "speech_input_manager.h"

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
  void SetRecognitionResult(int render_view_id, const string16& result);
  void DidCompleteRecording(int render_view_id);
  void DidCompleteRecognition(int render_view_id);

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled.
  bool OnMessageReceived(const IPC::Message& msg, bool* msg_was_ok);

  // Factory setter useful for tests.
  static void set_manager_factory(SpeechInputManager::FactoryMethod* factory) {
    manager_factory_ = factory;
  }

 private:
  friend class base::RefCountedThreadSafe<SpeechInputDispatcherHost>;
  virtual ~SpeechInputDispatcherHost();
  void SendMessageToRenderView(IPC::Message* message, int render_view_id);

  void OnStartRecognition(int render_view_id);
  void OnCancelRecognition(int render_view_id);
  void OnStopRecording(int render_view_id);

  // Returns the speech input manager to forward events to, creating one if
  // needed.
  SpeechInputManager* manager();

  int resource_message_filter_process_id_;

  scoped_ptr<SpeechInputManager> manager_;

  static SpeechInputManager::FactoryMethod* manager_factory_;

  DISALLOW_COPY_AND_ASSIGN(SpeechInputDispatcherHost);
};

}  // namespace speech_input

#endif  // CHROME_BROWSER_SPEECH_SPEECH_INPUT_DISPATCHER_HOST_H_
