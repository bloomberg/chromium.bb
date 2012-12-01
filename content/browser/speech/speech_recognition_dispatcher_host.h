// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_DISPATCHER_HOST_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "net/url_request/url_request_context_getter.h"

struct SpeechRecognitionHostMsg_StartRequest_Params;

namespace content {

class SpeechRecognitionManager;
class SpeechRecognitionPreferences;
struct SpeechRecognitionResult;

// SpeechRecognitionDispatcherHost is a delegate for Speech API messages used by
// RenderMessageFilter. Basically it acts as a proxy, relaying the events coming
// from the SpeechRecognitionManager to IPC messages (and vice versa).
// It's the complement of SpeechRecognitionDispatcher (owned by RenderView).
class CONTENT_EXPORT SpeechRecognitionDispatcherHost
    : public BrowserMessageFilter,
      public SpeechRecognitionEventListener {
 public:
  SpeechRecognitionDispatcherHost(
      int render_process_id,
      net::URLRequestContextGetter* context_getter,
      SpeechRecognitionPreferences* recognition_preferences);

  // SpeechRecognitionEventListener methods.
  virtual void OnRecognitionStart(int session_id) OVERRIDE;
  virtual void OnAudioStart(int session_id) OVERRIDE;
  virtual void OnEnvironmentEstimationComplete(int session_id) OVERRIDE;
  virtual void OnSoundStart(int session_id) OVERRIDE;
  virtual void OnSoundEnd(int session_id) OVERRIDE;
  virtual void OnAudioEnd(int session_id) OVERRIDE;
  virtual void OnRecognitionEnd(int session_id) OVERRIDE;
  virtual void OnRecognitionResults(
      int session_id,
      const SpeechRecognitionResults& results) OVERRIDE;
  virtual void OnRecognitionError(
      int session_id,
      const SpeechRecognitionError& error) OVERRIDE;
  virtual void OnAudioLevelsChange(int session_id,
                                   float volume,
                                   float noise_volume) OVERRIDE;

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // Singleton manager setter useful for tests.
  static void SetManagerForTests(SpeechRecognitionManager* manager);

 private:
  virtual ~SpeechRecognitionDispatcherHost();

  void OnStartRequest(
      const SpeechRecognitionHostMsg_StartRequest_Params& params);
  void OnAbortRequest(int render_view_id, int request_id);
  void OnStopCaptureRequest(int render_view_id, int request_id);

  // Returns the speech recognition manager to forward requests to.
  SpeechRecognitionManager* manager();

  int render_process_id_;
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  scoped_refptr<SpeechRecognitionPreferences> recognition_preferences_;

  static SpeechRecognitionManager* manager_for_tests_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_DISPATCHER_HOST_H_
