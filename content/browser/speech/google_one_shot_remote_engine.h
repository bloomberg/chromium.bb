// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_GOOGLE_ONE_SHOT_REMOTE_ENGINE_H_
#define CONTENT_BROWSER_SPEECH_GOOGLE_ONE_SHOT_REMOTE_ENGINE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/speech/audio_encoder.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/common/content_export.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLRequestContextGetter;
}

namespace content {

// Implements a SpeechRecognitionEngine by means of remote interaction with
// Google speech recognition webservice.
class CONTENT_EXPORT GoogleOneShotRemoteEngine
    : public NON_EXPORTED_BASE(SpeechRecognitionEngine),
      public net::URLFetcherDelegate {
 public:
  // Duration of each audio packet.
  static const int kAudioPacketIntervalMs;
  // ID passed to URLFetcher::Create(). Used for testing.
  static int url_fetcher_id_for_tests;

  explicit GoogleOneShotRemoteEngine(net::URLRequestContextGetter* context);
  ~GoogleOneShotRemoteEngine() override;

  // SpeechRecognitionEngine methods.
  void SetConfig(const SpeechRecognitionEngineConfig& config) override;
  void StartRecognition() override;
  void EndRecognition() override;
  void TakeAudioChunk(const AudioChunk& data) override;
  void AudioChunksEnded() override;
  bool IsRecognitionPending() const override;
  int GetDesiredAudioChunkDurationMs() const override;

  // net::URLFetcherDelegate methods.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

 private:
  SpeechRecognitionEngineConfig config_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
  scoped_refptr<net::URLRequestContextGetter> url_context_;
  scoped_ptr<AudioEncoder> encoder_;

  DISALLOW_COPY_AND_ASSIGN(GoogleOneShotRemoteEngine);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_GOOGLE_ONE_SHOT_REMOTE_ENGINE_H_
