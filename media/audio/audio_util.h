// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_UTIL_H_
#define MEDIA_AUDIO_AUDIO_UTIL_H_

#include "base/basictypes.h"
#include "build/build_config.h"
#include "media/base/media_export.h"

namespace media {

// Returns user buffer size as specified on the command line or 0 if no buffer
// size has been specified.
MEDIA_EXPORT int GetUserBufferSize();

// Computes a buffer size based on the given |sample_rate|. Must be used in
// conjunction with AUDIO_PCM_LINEAR.
MEDIA_EXPORT size_t GetHighLatencyOutputBufferSize(int sample_rate);

#if defined(OS_WIN)

// Returns number of buffers to be used by wave out.
MEDIA_EXPORT int NumberOfWaveOutBuffers();

#endif  // defined(OS_WIN)

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_UTIL_H_
