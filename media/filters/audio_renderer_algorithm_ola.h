// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioRendererAlgorithmOLA [ARAO] is the pitch-preservation implementation of
// AudioRendererAlgorithmBase [ARAB]. For speeds greater than 1.0f, FillBuffer()
// consumes more input data than output data requested and crossfades samples
// to fill |buffer_out|. For speeds less than 1.0f, FillBuffer() consumers less
// input data than output data requested and draws overlapping samples from the
// input data to fill |buffer_out|. ARAO will mute the audio for very high or
// very low playback rates to preserve quality. As ARAB is thread-unsafe, so is
// ARAO.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_OLA_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_OLA_H_

#include "base/gtest_prod_util.h"
#include "media/filters/audio_renderer_algorithm_base.h"

namespace media {

class MEDIA_EXPORT AudioRendererAlgorithmOLA
    : public AudioRendererAlgorithmBase {
 public:
  AudioRendererAlgorithmOLA();
  virtual ~AudioRendererAlgorithmOLA();

  // AudioRendererAlgorithmBase implementation
  virtual uint32 FillBuffer(uint8* dest, uint32 length);

  virtual void set_playback_rate(float new_rate);

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioRendererAlgorithmOLATest,
                           FillBuffer_NormalRate);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererAlgorithmOLATest,
                           FillBuffer_DoubleRate);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererAlgorithmOLATest,
                           FillBuffer_HalfRate);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererAlgorithmOLATest,
                           FillBuffer_QuarterRate);

  // Aligns |value| to a channel and sample boundary.
  void AlignToSampleBoundary(uint32* value);

  // Crossfades |samples| samples of |dest| with the data in |src|. Assumes
  // there is room in |dest| and enough data in |src|. Type is the datatype
  // of a data point in the waveform (i.e. uint8, int16, int32, etc). Also,
  // sizeof(one sample) == sizeof(Type) * channels.
  template <class Type>
  void Crossfade(int samples, const Type* src, Type* dest);

  // Members for ease of calculation in FillBuffer(). These members are based
  // on |playback_rate_|, but are stored separately so they don't have to be
  // recalculated on every call to FillBuffer().
  uint32 input_step_;
  uint32 output_step_;

  // Length for crossfade in bytes.
  uint32 crossfade_size_;

  // Window size, in bytes (calculated from audio properties).
  uint32 window_size_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererAlgorithmOLA);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_OLA_H_
