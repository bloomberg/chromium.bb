// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_DISPATCHER_HOST_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/speech_recognizer.mojom.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {

class SpeechRecognitionManager;

// SpeechRecognitionDispatcherHost is a delegate for Speech API messages used by
// RenderMessageFilter. Basically it acts as a proxy, relaying the events coming
// from the SpeechRecognitionManager to IPC messages (and vice versa).
// It's the complement of SpeechRecognitionDispatcher (owned by RenderFrame).
class CONTENT_EXPORT SpeechRecognitionDispatcherHost
    : public mojom::SpeechRecognizer,
      public SpeechRecognitionEventListener {
 public:
  SpeechRecognitionDispatcherHost(
      int render_process_id,
      int render_frame_id,
      scoped_refptr<net::URLRequestContextGetter> context_getter,
      // WeakPtr can only be used on the UI thread.
      base::WeakPtr<IPC::Sender> sender);
  ~SpeechRecognitionDispatcherHost() override;
  static void Create(int render_process_id,
                     int render_frame_id,
                     scoped_refptr<net::URLRequestContextGetter> context_getter,
                     base::WeakPtr<IPC::Sender>,
                     mojom::SpeechRecognizerRequest request);
  base::WeakPtr<SpeechRecognitionDispatcherHost> AsWeakPtr();

  // mojom::SpeechRecognizer implementation
  void StartRequest(
      mojom::StartSpeechRecognitionRequestParamsPtr params) override;
  // TODO(adithyas): Once we convert browser -> renderer messages to mojo
  // (https://crbug.com/781655) and replace |request_id| with a mojo
  // InterfacePtr, we shouldn't need AbortRequest and AbortAllRequests.
  void AbortRequest(int32_t request_id) override;
  void AbortAllRequests() override;
  void StopCaptureRequest(int32_t request_id) override;

  // SpeechRecognitionEventListener methods.
  void OnRecognitionStart(int session_id) override;
  void OnAudioStart(int session_id) override;
  void OnEnvironmentEstimationComplete(int session_id) override;
  void OnSoundStart(int session_id) override;
  void OnSoundEnd(int session_id) override;
  void OnAudioEnd(int session_id) override;
  void OnRecognitionEnd(int session_id) override;
  void OnRecognitionResults(int session_id,
                            const SpeechRecognitionResults& results) override;
  void OnRecognitionError(int session_id,
                          const SpeechRecognitionError& error) override;
  void OnAudioLevelsChange(int session_id,
                           float volume,
                           float noise_volume) override;

 private:
  friend class base::DeleteHelper<SpeechRecognitionDispatcherHost>;
  friend class BrowserThread;

  static void StartRequestOnUI(
      base::WeakPtr<SpeechRecognitionDispatcherHost>
          speech_recognition_dispatcher_host,
      int render_process_id,
      int render_frame_id,
      mojom::StartSpeechRecognitionRequestParamsPtr params);
  void StartSessionOnIO(mojom::StartSpeechRecognitionRequestParamsPtr params,
                        int embedder_render_process_id,
                        int embedder_render_frame_id,
                        bool filter_profanities);
  // Sends the given message on the UI thread. Can be called from the IO thread.
  void Send(IPC::Message* message);

  int render_process_id_;
  int render_frame_id_;
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // sender_ can only be accessed on the UI thread. To send messages from the IO
  // thread, use SpeechRecognitionDispatcherHost::Send.
  base::WeakPtr<IPC::Sender> sender_;

  // Used for posting asynchronous tasks (on the IO thread) without worrying
  // about this class being destroyed in the meanwhile (due to browser shutdown)
  // since tasks pending on a destroyed WeakPtr are automatically discarded.
  base::WeakPtrFactory<SpeechRecognitionDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_DISPATCHER_HOST_H_
