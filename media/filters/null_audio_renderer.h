// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_NULL_AUDIO_RENDERER_H_
#define MEDIA_FILTERS_NULL_AUDIO_RENDERER_H_

// NullAudioRenderer effectively uses an extra thread to "throw away" the
// audio data at a rate resembling normal playback speed.  It's just like
// decoding to /dev/null!
//
// NullAudioRenderer can also be used in situations where the client has no
// audio device or we haven't written an audio implementation for a particular
// platform yet.

#include <deque>

#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "media/base/buffers.h"
#include "media/base/filters.h"
#include "media/filters/audio_renderer_base.h"

namespace media {

class MEDIA_EXPORT NullAudioRenderer
    : public AudioRendererBase,
      public base::PlatformThread::Delegate {
 public:
  NullAudioRenderer();
  virtual ~NullAudioRenderer();

  // AudioRenderer implementation.
  virtual void SetVolume(float volume);

  // PlatformThread::Delegate implementation.
  virtual void ThreadMain();

 protected:
  // AudioRendererBase implementation.
  virtual bool OnInitialize(const AudioDecoderConfig& config);
  virtual void OnStop();

 private:
  // A number to convert bytes written in FillBuffer to milliseconds based on
  // the audio format.
  size_t bytes_per_millisecond_;

  // A buffer passed to FillBuffer to advance playback.
  scoped_array<uint8> buffer_;
  size_t buffer_size_;

  // Separate thread used to throw away data.
  base::PlatformThreadHandle thread_;

  // Shutdown flag.
  bool shutdown_;

  DISALLOW_COPY_AND_ASSIGN(NullAudioRenderer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_NULL_AUDIO_RENDERER_H_
