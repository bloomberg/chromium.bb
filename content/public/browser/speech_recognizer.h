// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNIZER_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNIZER_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace net {
class URLRequestContextGetter;
}

namespace content {

class SpeechRecognitionEventListener;

// Records audio, sends recorded audio to server and translates server response
// to recognition result.
// TODO(primiano) remove this public interface as soon as we fix speech input
// extensions.
class SpeechRecognizer : public base::RefCountedThreadSafe<SpeechRecognizer> {
 public:
  CONTENT_EXPORT static SpeechRecognizer* Create(
      SpeechRecognitionEventListener* event_listener,
      int session_id,
      const std::string& language,
      const std::string& grammar,
      net::URLRequestContextGetter* context_getter,
      bool filter_profanities,
      const std::string& hardware_info,
      const std::string& origin_url);

  // Starts audio recording and the recognition process. The same
  // SpeechRecognizer instance can be used multiple times for speech recognition
  // though each recognition request can be made only after the previous one
  // completes (i.e. after receiving
  // SpeechRecognitionEventListener::OnRecognitionEnd).
  virtual void StartRecognition() = 0;

  // Stops recording audio and cancels recognition. Any audio recorded so far
  // gets discarded.
  virtual void AbortRecognition() = 0;

  // Stops recording audio and finalizes recognition, possibly getting results.
  virtual void StopAudioCapture() = 0;

  // Checks wether the recognizer is active, that is, either capturing audio
  // or waiting for a result.
  virtual bool IsActive() const = 0;

  // Checks whether the recognizer is capturing audio.
  virtual bool IsCapturingAudio() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<SpeechRecognizer>;
  virtual ~SpeechRecognizer() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNIZER_H_
