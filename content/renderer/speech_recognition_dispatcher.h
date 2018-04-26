// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SPEECH_RECOGNITION_DISPATCHER_H_
#define CONTENT_RENDERER_SPEECH_RECOGNITION_DISPATCHER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "content/common/speech_recognizer.mojom.h"
#include "content/public/common/speech_recognition_result.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/web/web_speech_recognition_handle.h"
#include "third_party/blink/public/web/web_speech_recognizer.h"
#include "third_party/blink/public/web/web_speech_recognizer_client.h"

namespace content {
struct SpeechRecognitionError;

// SpeechRecognitionDispatcher is a delegate for methods used by WebKit for
// scripted JS speech APIs. It's the complement of
// SpeechRecognitionDispatcherHost (owned by RenderFrameHost).
class SpeechRecognitionDispatcher : public RenderFrameObserver,
                                    public blink::WebSpeechRecognizer {
 public:
  explicit SpeechRecognitionDispatcher(RenderFrame* render_frame);
  ~SpeechRecognitionDispatcher() override;

 private:
  using HandleMap = std::map<int, blink::WebSpeechRecognitionHandle>;

  // RenderFrameObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() override;
  void WasHidden() override;

  // blink::WebSpeechRecognizer implementation.
  void Start(const blink::WebSpeechRecognitionHandle&,
             const blink::WebSpeechRecognitionParams&,
             const blink::WebSpeechRecognizerClient&) override;
  void Stop(const blink::WebSpeechRecognitionHandle&,
            const blink::WebSpeechRecognizerClient&) override;
  void Abort(const blink::WebSpeechRecognitionHandle&,
             const blink::WebSpeechRecognizerClient&) override;

  void OnRecognitionStarted(int request_id);
  void OnAudioStarted(int request_id);
  void OnSoundStarted(int request_id);
  void OnSoundEnded(int request_id);
  void OnAudioEnded(int request_id);
  void OnErrorOccurred(int request_id, const SpeechRecognitionError& error);
  void OnRecognitionEnded(int request_id);
  void OnResultsRetrieved(int request_id,
                          const SpeechRecognitionResults& result);

  int GetOrCreateIDForHandle(const blink::WebSpeechRecognitionHandle& handle);
  bool HandleExists(const blink::WebSpeechRecognitionHandle& handle);
  HandleMap::iterator FindHandleInMap(
      const blink::WebSpeechRecognitionHandle& handle);
  const blink::WebSpeechRecognitionHandle& GetHandleFromID(int handle_id);

  mojom::SpeechRecognizer& GetSpeechRecognitionHost();

  mojom::SpeechRecognizerPtr speech_recognition_host_;

  // The Blink client class that we use to send events back to the JS world.
  blink::WebSpeechRecognizerClient recognizer_client_;

  // This maps between request id values and the Blink handle values.
  HandleMap handle_map_;
  int next_id_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SPEECH_RECOGNITION_DISPATCHER_H_
