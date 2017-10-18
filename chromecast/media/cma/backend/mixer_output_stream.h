// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_OUTPUT_STREAM_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_OUTPUT_STREAM_H_

#include <memory>

#include "chromecast/public/media/media_pipeline_backend.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace chromecast {
namespace media {

// Interface for output audio stream. Used by the mixer to play mixed stream.
class MixerOutputStream {
 public:
  static constexpr int kInvalidSampleRate = 0;

  // Creates default MixerOutputStream.
  static std::unique_ptr<MixerOutputStream> Create();

  virtual ~MixerOutputStream() {}

  // Returns true if the sample rate is fixed.
  virtual bool IsFixedSampleRate() = 0;

  // Start the stream. Caller must call GetSampleRate() to get the actual sample
  // rate selected for the stream. It may be different from
  // |requested_sample_rate|, e.g. if IsFixedSampleRate() is true, or the device
  // doesn't support |requested_sample_rate|.
  virtual bool Start(int requested_sample_rate, int channels) = 0;

  // Returns current sample rate.
  virtual int GetSampleRate() = 0;

  // Returns current rendering delay for the stream.
  virtual MediaPipelineBackend::AudioDecoder::RenderingDelay
  GetRenderingDelay() = 0;

  // Return how much time is left to call Write() to prevent buffer underrun.
  virtual bool GetTimeUntilUnderrun(base::TimeDelta* result) = 0;

  // |data_size| is size of |data|. Should be divided by number
  // of channels to get number of frames.
  virtual bool Write(const float* data,
                     int data_size,
                     bool* out_playback_interrupted) = 0;

  // Stops the stream. After being stopped the stream can be restarted by
  // calling Start().
  virtual void Stop() = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_OUTPUT_STREAM_H_
