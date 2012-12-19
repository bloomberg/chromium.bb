// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_IOS_AUDIO_SESSION_UTIL_IOS_H_
#define MEDIA_AUDIO_IOS_AUDIO_SESSION_UTIL_IOS_H_

namespace media {

// Initializes and configures the audio session, returning a bool indicating
// whether initialization was successful. Can be called multiple times.
// Safe to call from any thread.
bool InitAudioSessionIOS();

}  // namespace media

#endif  // MEDIA_AUDIO_IOS_AUDIO_SESSION_UTIL_IOS_H_
