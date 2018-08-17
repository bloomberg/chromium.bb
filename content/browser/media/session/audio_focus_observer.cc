// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_observer.h"

#include "build/build_config.h"
#include "content/browser/media/session/audio_focus_manager.h"

namespace content {

AudioFocusObserver::AudioFocusObserver() {
#if !defined(OS_ANDROID)
  // TODO(https://crbug.com/873320): Add support for Android.
  AudioFocusManager::GetInstance()->AddObserver(this);
#endif
}

AudioFocusObserver::~AudioFocusObserver() {
#if !defined(OS_ANDROID)
  // TODO(https://crbug.com/873320): Add support for Android.
  AudioFocusManager::GetInstance()->RemoveObserver(this);
#endif
}

}  // namespace content
