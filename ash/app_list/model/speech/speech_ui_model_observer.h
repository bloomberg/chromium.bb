// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_SPEECH_SPEECH_UI_MODEL_OBSERVER_H_
#define ASH_APP_LIST_MODEL_SPEECH_SPEECH_UI_MODEL_OBSERVER_H_

#include <stdint.h>

#include "ash/app_list/model/app_list_model_export.h"
#include "ash/app_list/model/speech/speech_ui_model.h"
#include "base/strings/string16.h"

namespace app_list {

class APP_LIST_MODEL_EXPORT SpeechUIModelObserver {
 public:
  // Invoked when sound level for the speech recognition has changed. |level|
  // represents the current sound-level in the range of [0, 255].
  virtual void OnSpeechSoundLevelChanged(uint8_t level) {}

  // Invoked when a speech result arrives. |is_final| is true only when the
  // speech result is final.
  virtual void OnSpeechResult(const base::string16& result, bool is_final) {}

  // Invoked when the state of speech recognition is changed.
  virtual void OnSpeechRecognitionStateChanged(
      SpeechRecognitionState new_state) {}

 protected:
  virtual ~SpeechUIModelObserver() {}
};

}  // namespace app_list

#endif  // ASH_APP_LIST_MODEL_SPEECH_SPEECH_UI_MODEL_OBSERVER_H_
