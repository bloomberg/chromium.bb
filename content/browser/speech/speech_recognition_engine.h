// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_ENGINE_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_ENGINE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace content {
struct SpeechRecognitionResult;
struct SpeechRecognitionError;
}

namespace speech {

class AudioChunk;

// This interface models the basic contract that a speech recognition engine,
// either working locally or relying on a remote web-service, must obey.
// The expected call sequence for exported methods is:
// StartRecognition      Mandatory at beginning of SR.
//   TakeAudioChunk      For every audio chunk pushed.
//   AudioChunksEnded    Finalize the audio stream (omitted in case of errors).
// EndRecognition        Mandatory at end of SR (even on errors).
// No delegate callback is allowed before Initialize() or after Cleanup().
class SpeechRecognitionEngine {
 public:
  // Interface for receiving callbacks from this object.
  class Delegate {
   public:
    // Called whenever a result is retrieved. It might be issued several times,
    // (e.g., in the case of continuous speech recognition engine
    // implementations).
    virtual void OnSpeechRecognitionEngineResult(
        const content::SpeechRecognitionResult& result) = 0;
    virtual void OnSpeechRecognitionEngineError(
        const content::SpeechRecognitionError& error) = 0;

   protected:
    virtual ~Delegate() {}
  };

  virtual ~SpeechRecognitionEngine() {}

  // Called when the speech recognition begins, before any TakeAudioChunk call.
  virtual void StartRecognition() = 0;

  // End any recognition activity and don't make any further callback.
  // Must be always called to close the corresponding StartRecognition call,
  // even in case of errors.
  // No further TakeAudioChunk/AudioChunksEnded calls are allowed after this.
  virtual void EndRecognition() = 0;

  // Push a chunk of uncompressed audio data, where the chunk length agrees with
  // GetDesiredAudioChunkDurationMs().
  virtual void TakeAudioChunk(const AudioChunk& data) = 0;

  // Notifies the engine that audio capture has completed and no more chunks
  // will be pushed. The engine, however, can still provide further results
  // using the audio chunks collected so far.
  virtual void AudioChunksEnded() = 0;

  // Checks wheter recognition of pushed audio data is pending.
  virtual bool IsRecognitionPending() const = 0;

  // Retrieves the desired duration, in milliseconds, of pushed AudioChunk(s).
  virtual int GetDesiredAudioChunkDurationMs() const = 0;

  // set_delegate detached from constructor for lazy dependency injection.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

 protected:
  Delegate* delegate() const { return delegate_; }

 private:
  Delegate* delegate_;
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechRecognitionEngine::Delegate SpeechRecognitionEngineDelegate;

}  // namespace speech

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_ENGINE_H_
