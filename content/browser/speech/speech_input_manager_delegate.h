// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_DELEGATE_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_DELEGATE_H_
#pragma once

namespace content {

struct SpeechInputResult;

// Implemented by the dispatcher host to relay events to the render views.
class SpeechInputManagerDelegate {
 public:
  virtual void SetRecognitionResult(int caller_id,
                                    const SpeechInputResult& result) = 0;

  virtual void DidCompleteRecording(int caller_id) = 0;

  virtual void DidCompleteRecognition(int caller_id) = 0;

 protected:
  virtual ~SpeechInputManagerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_DELEGATE_H_
