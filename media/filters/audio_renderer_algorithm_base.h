// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioRendererAlgorithmBase buffers and transforms audio data. The owner of
// this object provides audio data to the object through EnqueueBuffer() and
// requests data from the buffer via FillBuffer(). The owner also sets the
// playback rate, and the AudioRendererAlgorithm will stretch or compress the
// buffered audio as necessary to match the playback rate when fulfilling
// FillBuffer() requests. AudioRendererAlgorithm can request more data to be
// buffered via a read callback passed in during initialization.
//
// This class is *not* thread-safe. Calls to enqueue and retrieve data must be
// locked if called from multiple threads.
//
// AudioRendererAlgorithmBase uses a simple pitch-preservation algorithm to
// stretch and compress audio data to meet playback speeds less than and
// greater than the natural playback of the audio stream.
//
// Audio at very low or very high playback rates are muted to preserve quality.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_BASE_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_BASE_H_

#include <deque>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/seekable_buffer.h"

namespace media {

class Buffer;

class MEDIA_EXPORT AudioRendererAlgorithmBase {
 public:
  AudioRendererAlgorithmBase();
  ~AudioRendererAlgorithmBase();

  // Checks validity of audio parameters.
  void Initialize(
      int channels, int sample_rate, int sample_bits,
      float initial_playback_rate, const base::Closure& callback);

  // Tries to fill |length| bytes of |dest| with possibly scaled data from
  // our |queue_|. Returns the number of bytes copied into |dest|.
  uint32 FillBuffer(uint8* dest, uint32 length);

  // Clears |queue_|.
  void FlushBuffers();

  // Returns the time of the next byte in our data or kNoTimestamp if current
  // time is unknown.
  base::TimeDelta GetTime();

  // Enqueues a buffer. It is called from the owner of the algorithm after a
  // read completes.
  void EnqueueBuffer(Buffer* buffer_in);

  // Getter/setter for |playback_rate_|.
  float playback_rate();
  void SetPlaybackRate(float new_rate);

  // Returns whether |queue_| is empty.
  bool IsQueueEmpty();

  // Returns true if we have enough data
  bool IsQueueFull();

  // Returns the number of bytes left in |queue_|, which may be larger than
  // QueueCapacity() in the event that a read callback delivered more data than
  // |queue_| was intending to hold.
  uint32 QueueSize();

  // Returns the capacity of |queue_|.
  uint32 QueueCapacity();

  // Increase the capacity of |queue_| if possible.
  void IncreaseQueueCapacity();

 private:
  // Advances |queue_|'s internal pointer by |bytes|.
  void AdvanceInputPosition(uint32 bytes);

  // Tries to copy |bytes| bytes from |queue_| to |dest|. Returns the number of
  // bytes successfully copied.
  uint32 CopyFromInput(uint8* dest, uint32 bytes);

  // Converts a duration in milliseconds to a byte count based on
  // the current sample rate, channel count, and bytes per sample.
  int DurationToBytes(int duration_in_milliseconds) const;

  FRIEND_TEST_ALL_PREFIXES(AudioRendererAlgorithmBaseTest,
                           FillBuffer_NormalRate);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererAlgorithmBaseTest,
                           FillBuffer_DoubleRate);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererAlgorithmBaseTest,
                           FillBuffer_HalfRate);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererAlgorithmBaseTest,
                           FillBuffer_QuarterRate);

  // Aligns |value| to a channel and sample boundary.
  void AlignToSampleBoundary(uint32* value);

  // Crossfades |samples| samples of |dest| with the data in |src|. Assumes
  // there is room in |dest| and enough data in |src|. Type is the datatype
  // of a data point in the waveform (i.e. uint8, int16, int32, etc). Also,
  // sizeof(one sample) == sizeof(Type) * channels.
  template <class Type>
  void Crossfade(int samples, const Type* src, Type* dest);

  // Audio properties.
  int channels_;
  int sample_rate_;
  int sample_bytes_;

  // Used by algorithm to scale output.
  float playback_rate_;

  // Used to request more data.
  base::Closure request_read_callback_;

  // Queued audio data.
  SeekableBuffer queue_;

  // Largest capacity queue_ can grow to.
  size_t max_queue_capacity_;

  // Members for ease of calculation in FillBuffer(). These members are based
  // on |playback_rate_|, but are stored separately so they don't have to be
  // recalculated on every call to FillBuffer().
  uint32 input_step_;
  uint32 output_step_;

  // Length for crossfade in bytes.
  uint32 crossfade_size_;

  // Window size, in bytes (calculated from audio properties).
  uint32 window_size_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererAlgorithmBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_BASE_H_
