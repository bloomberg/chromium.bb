// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_GOOGLE_ONE_SHOT_REMOTE_ENGINE_H_
#define CONTENT_BROWSER_SPEECH_GOOGLE_ONE_SHOT_REMOTE_ENGINE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/speech/audio_encoder.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/common/content_export.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

class URLFetcher;

namespace content {
struct SpeechRecognitionResult;
}

namespace net {
class URLRequestContextGetter;
}

namespace speech {

class AudioChunk;

struct CONTENT_EXPORT GoogleOneShotRemoteEngineConfig {
  std::string language;
  std::string grammar;
  bool filter_profanities;
  std::string hardware_info;
  std::string origin_url;
  int audio_sample_rate;
  int audio_num_bits_per_sample;

  GoogleOneShotRemoteEngineConfig();
  ~GoogleOneShotRemoteEngineConfig();
};

// Implements a SpeechRecognitionEngine by means of remote interaction with
// Google speech recognition webservice.
class CONTENT_EXPORT GoogleOneShotRemoteEngine
    : public NON_EXPORTED_BASE(SpeechRecognitionEngine),
      public content::URLFetcherDelegate {
 public:
  // Duration of each audio packet.
  static const int kAudioPacketIntervalMs;
  // ID passed to URLFetcher::Create(). Used for testing.
  static int url_fetcher_id_for_tests;

  explicit GoogleOneShotRemoteEngine(net::URLRequestContextGetter* context);
  virtual ~GoogleOneShotRemoteEngine();
  void SetConfig(const GoogleOneShotRemoteEngineConfig& config);

  // SpeechRecognitionEngine methods.
  virtual void StartRecognition() OVERRIDE;
  virtual void EndRecognition() OVERRIDE;
  virtual void TakeAudioChunk(const AudioChunk& data) OVERRIDE;
  virtual void AudioChunksEnded() OVERRIDE;
  virtual bool IsRecognitionPending() const OVERRIDE;
  virtual int GetDesiredAudioChunkDurationMs() const OVERRIDE;

  // content::URLFetcherDelegate methods.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  GoogleOneShotRemoteEngineConfig config_;
  scoped_ptr<content::URLFetcher> url_fetcher_;
  scoped_refptr<net::URLRequestContextGetter> url_context_;
  scoped_ptr<AudioEncoder> encoder_;

  DISALLOW_COPY_AND_ASSIGN(GoogleOneShotRemoteEngine);
};

}  // namespace speech

#endif  // CONTENT_BROWSER_SPEECH_GOOGLE_ONE_SHOT_REMOTE_ENGINE_H_
