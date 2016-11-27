// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SPEECH_RECOGNIZER_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_SPEECH_RECOGNIZER_DELEGATE_H_

#include <stdint.h>
#include <string>

#include "base/strings/string16.h"
#include "ui/app_list/speech_ui_model_observer.h"

namespace app_list {

// Delegate for the app list speech recognizer. All methods are called from the
// UI thread.
class SpeechRecognizerDelegate {
 public:
  // Receive a speech recognition result. |is_final| indicated whether the
  // result is an intermediate or final result. If |is_final| is true, then the
  // recognizer stops and no more results will be returned.
  virtual void OnSpeechResult(const base::string16& query, bool is_final) = 0;

  // Invoked regularly to indicate the average sound volume.
  virtual void OnSpeechSoundLevelChanged(int16_t level) = 0;

  // Invoked when the state of speech recognition is changed.
  virtual void OnSpeechRecognitionStateChanged(
      SpeechRecognitionState new_state) = 0;

  // Get the OAuth2 scope and token to pass to the speech recognizer. Does not
  // modify the arguments if no auth token is available or allowed.
  virtual void GetSpeechAuthParameters(std::string* auth_scope,
                                       std::string* auth_token) = 0;

 protected:
  virtual ~SpeechRecognizerDelegate() {}
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SPEECH_RECOGNIZER_DELEGATE_H_
