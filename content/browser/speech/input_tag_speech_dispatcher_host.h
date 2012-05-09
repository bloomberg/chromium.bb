// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_INPUT_TAG_SPEECH_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SPEECH_INPUT_TAG_SPEECH_DISPATCHER_HOST_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "net/url_request/url_request_context_getter.h"

struct InputTagSpeechHostMsg_StartRecognition_Params;

namespace content {
class SpeechRecognitionPreferences;
struct SpeechRecognitionResult;
}

namespace speech {

class SpeechRecognitionManagerImpl;

// InputTagSpeechDispatcherHost is a delegate for Speech API messages used by
// RenderMessageFilter. Basically it acts as a proxy, relaying the events coming
// from the SpeechRecognitionManager to IPC messages (and vice versa).
// It's the complement of SpeechRecognitionDispatcher (owned by RenderView).
class CONTENT_EXPORT InputTagSpeechDispatcherHost
    : public content::BrowserMessageFilter,
      public content::SpeechRecognitionEventListener {
 public:
  InputTagSpeechDispatcherHost(
      int render_process_id,
      net::URLRequestContextGetter* url_request_context_getter,
      content::SpeechRecognitionPreferences* recognition_preferences);

  // SpeechRecognitionEventListener methods.
  virtual void OnRecognitionStart(int session_id) OVERRIDE;
  virtual void OnAudioStart(int session_id) OVERRIDE;
  virtual void OnEnvironmentEstimationComplete(int session_id) OVERRIDE;
  virtual void OnSoundStart(int session_id) OVERRIDE;
  virtual void OnSoundEnd(int session_id) OVERRIDE;
  virtual void OnAudioEnd(int session_id) OVERRIDE;
  virtual void OnRecognitionEnd(int session_id) OVERRIDE;
  virtual void OnRecognitionResult(
      int session_id, const content::SpeechRecognitionResult& result) OVERRIDE;
  virtual void OnRecognitionError(
      int session_id, const content::SpeechRecognitionError& error) OVERRIDE;
  virtual void OnAudioLevelsChange(
      int session_id, float volume, float noise_volume) OVERRIDE;

  // content::BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // Singleton manager setter useful for tests.
  static void set_manager(SpeechRecognitionManagerImpl* manager);

 private:
  virtual ~InputTagSpeechDispatcherHost();

  void OnStartRecognition(
      const InputTagSpeechHostMsg_StartRecognition_Params &params);
  void OnCancelRecognition(int render_view_id, int request_id);
  void OnStopRecording(int render_view_id, int request_id);

  // Returns the speech recognition manager to forward events to, creating one
  // if needed.
  SpeechRecognitionManagerImpl* manager();

  int render_process_id_;
  bool may_have_pending_requests_;  // Set if we received any speech IPC request

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  scoped_refptr<content::SpeechRecognitionPreferences> recognition_preferences_;

  static SpeechRecognitionManagerImpl* manager_;

  DISALLOW_COPY_AND_ASSIGN(InputTagSpeechDispatcherHost);
};

}  // namespace speech

#endif  // CONTENT_BROWSER_SPEECH_INPUT_TAG_SPEECH_DISPATCHER_HOST_H_
