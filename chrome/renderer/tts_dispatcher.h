// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_TTS_DISPATCHER_H_
#define CHROME_RENDERER_TTS_DISPATCHER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "content/public/renderer/render_process_observer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSpeechSynthesizer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSpeechSynthesizerClient.h"

namespace IPC {
class Message;
}

struct TtsVoice;

// TtsDispatcher is a delegate for methods used by WebKit for
// speech synthesis APIs. It's the complement of
// TtsDispatcherHost (owned by RenderViewHost).
class TtsDispatcher
    : public WebKit::WebSpeechSynthesizer,
      public content::RenderProcessObserver {
 public:
  explicit TtsDispatcher(WebKit::WebSpeechSynthesizerClient* client);

 private:
  virtual ~TtsDispatcher();

  // RenderProcessObserver override.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebKit::WebSpeechSynthesizer implementation.
  virtual void updateVoiceList() OVERRIDE;
  virtual void speak(const WebKit::WebSpeechSynthesisUtterance& utterance)
      OVERRIDE;
  virtual void pause() OVERRIDE;
  virtual void resume() OVERRIDE;
  virtual void cancel() OVERRIDE;

  WebKit::WebSpeechSynthesisUtterance FindUtterance(int utterance_id);

  void OnSetVoiceList(const std::vector<TtsVoice>& voices);
  void OnDidStartSpeaking(int utterance_id);
  void OnDidFinishSpeaking(int utterance_id);
  void OnDidPauseSpeaking(int utterance_id);
  void OnDidResumeSpeaking(int utterance_id);
  void OnWordBoundary(int utterance_id, int char_index);
  void OnSentenceBoundary(int utterance_id, int char_index);
  void OnMarkerEvent(int utterance_id, int char_index);
  void OnWasInterrupted(int utterance_id);
  void OnWasCancelled(int utterance_id);
  void OnSpeakingErrorOccurred(int utterance_id,
                               const std::string& error_message);

  // The WebKit client class that we use to send events back to the JS world.
  // Weak reference, this will be valid as long as this object exists.
  WebKit::WebSpeechSynthesizerClient* synthesizer_client_;

  // Next utterance id, used to map response IPCs to utterance objects.
  static int next_utterance_id_;

  // Map from id to utterance objects.
  base::hash_map<int, WebKit::WebSpeechSynthesisUtterance> utterance_id_map_;

  DISALLOW_COPY_AND_ASSIGN(TtsDispatcher);
};

#endif  // CHROME_RENDERER_TTS_DISPATCHER_H_
