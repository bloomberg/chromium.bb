// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_START_PAGE_OBSERVER_H_
#define CHROME_BROWSER_UI_APP_LIST_START_PAGE_OBSERVER_H_

#include <stdint.h>

#include "ash/app_list/model/speech/speech_ui_model.h"
#include "base/strings/string16.h"

namespace app_list {

class StartPageObserver {
 public:
  // Invoked when a sound level of speech recognition is changed.
  virtual void OnSpeechSoundLevelChanged(int16_t level) = 0;

  // Invoked when the online speech recognition state is changed.
  virtual void OnSpeechRecognitionStateChanged(
      SpeechRecognitionState new_state) = 0;

 protected:
  virtual ~StartPageObserver() {}
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_START_PAGE_OBSERVER_H_
