// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "media" command-line switches.

#ifndef MEDIA_BASE_MEDIA_SWITCHES_H_
#define MEDIA_BASE_MEDIA_SWITCHES_H_

#include "build/build_config.h"
#include "media/base/media_export.h"

namespace switches {

#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_SOLARIS)
extern const char kAlsaOutputDevice[];
extern const char kAlsaInputDevice[];
#endif

MEDIA_EXPORT extern const char kAudioBufferSize[];

#if defined(USE_CRAS)
MEDIA_EXPORT extern const char kUseCras[];
#endif

#if defined(OS_WIN)
MEDIA_EXPORT extern const char kEnableExclusiveAudio[];
MEDIA_EXPORT extern const char kForceWaveAudio[];
#endif

MEDIA_EXPORT extern const char kVideoThreads[];

MEDIA_EXPORT extern const char kDisableEncryptedMedia[];

MEDIA_EXPORT extern const char kEnableOpusPlayback[];

MEDIA_EXPORT extern const char kEnableVp9Playback[];

MEDIA_EXPORT extern const char kEnableVp8AlphaPlayback[];

MEDIA_EXPORT extern const char kEnableEac3Playback[];

#if defined(OS_WIN)
MEDIA_EXPORT extern const char kWaveOutBuffers[];
#endif

}  // namespace switches

#endif  // MEDIA_BASE_MEDIA_SWITCHES_H_
