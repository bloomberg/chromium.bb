// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_input_manager.h"

#include "content/browser/browser_thread.h"
#include "media/audio/audio_manager.h"

namespace speech_input {

SpeechInputManager::SpeechInputManager() : censor_results_(true) {
}

SpeechInputManager::~SpeechInputManager() {
}

void SpeechInputManager::ShowAudioInputSettings() {
  // Since AudioManager::ShowAudioInputSettings can potentially launch external
  // processes, do that in the FILE thread to not block the calling threads.
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableFunction(&SpeechInputManager::ShowAudioInputSettings));
    return;
  }

  DCHECK(AudioManager::GetAudioManager()->CanShowAudioInputSettings());
  if (AudioManager::GetAudioManager()->CanShowAudioInputSettings())
    AudioManager::GetAudioManager()->ShowAudioInputSettings();
}

}  // namespace speech_input
