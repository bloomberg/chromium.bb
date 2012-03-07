// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNIZER_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNIZER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace net {
class URLRequestContextGetter;
}

namespace content {

class SpeechRecognizerDelegate;

// Records audio, sends recorded audio to server and translates server response
// to recognition result.
class SpeechRecognizer : public base::RefCountedThreadSafe<SpeechRecognizer> {
 public:
  CONTENT_EXPORT static SpeechRecognizer* Create(
      SpeechRecognizerDelegate* delegate,
      int caller_id,
      const std::string& language,
      const std::string& grammar,
      net::URLRequestContextGetter* context_getter,
      bool filter_profanities,
      const std::string& hardware_info,
      const std::string& origin_url);

  virtual ~SpeechRecognizer() {}

  // Starts audio recording and does recognition after recording ends. The same
  // SpeechRecognizer instance can be used multiple times for speech recognition
  // though each recognition request can be made only after the previous one
  // completes (i.e. after receiving
  // SpeechRecognizerDelegate::DidCompleteRecognition).
  virtual bool StartRecording() = 0;

  // Stops recording audio and cancels recognition. Any audio recorded so far
  // gets discarded.
  virtual void CancelRecognition() = 0;
};

}  // namespace speech

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNIZER_H_
