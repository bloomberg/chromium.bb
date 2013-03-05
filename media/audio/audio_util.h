// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_UTIL_H_
#define MEDIA_AUDIO_AUDIO_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

namespace base {
class SharedMemory;
}

namespace media {
class AudioBus;

// For all audio functions 3 audio formats are supported:
// 8 bits unsigned 0 to 255.
// 16 bit signed (little endian).
// 32 bit signed (little endian)

// AdjustVolume() does a software volume adjustment of a sample buffer.
// The samples are multiplied by the volume, which should range from
// 0.0 (mute) to 1.0 (full volume).
// Using software allows each audio and video to have its own volume without
// affecting the master volume.
// In the future the function may be used to adjust the sample format to
// simplify hardware requirements and to support a wider variety of input
// formats.
// The buffer is modified in-place to avoid memory management, as this
// function may be called in performance critical code.
MEDIA_EXPORT bool AdjustVolume(void* buf,
                               size_t buflen,
                               int channels,
                               int bytes_per_sample,
                               float volume);

// Computes a buffer size based on the given |sample_rate|. Must be used in
// conjunction with AUDIO_PCM_LINEAR.
MEDIA_EXPORT size_t GetHighLatencyOutputBufferSize(int sample_rate);

#if defined(OS_WIN)

// Returns number of buffers to be used by wave out.
MEDIA_EXPORT int NumberOfWaveOutBuffers();

#endif  // defined(OS_WIN)

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_UTIL_H_
