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

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "media/base/seekable_buffer.h"

namespace media {

class Buffer;

class MEDIA_EXPORT AudioRendererAlgorithmBase {
 public:
  AudioRendererAlgorithmBase();
  ~AudioRendererAlgorithmBase();

  // Initializes this object with information about the audio stream.
  // |samples_per_second| is in Hz. |read_request_callback| is called to
  // request more data from the client, requests that are fulfilled through
  // calls to EnqueueBuffer().
  void Initialize(int channels,
                  int samples_per_second,
                  int bits_per_channel,
                  float initial_playback_rate,
                  const base::Closure& request_read_cb);

  // Tries to fill |length| bytes of |dest| with possibly scaled data from our
  // |audio_buffer_|. Data is scaled based on the playback rate, using a
  // variation of the Overlap-Add method to combine sample windows.
  //
  // Data from |audio_buffer_| is consumed in proportion to the playback rate.
  // FillBuffer() will fit |playback_rate_| * |length| bytes of raw data from
  // |audio_buffer| into |length| bytes of output data in |dest| by chopping up
  // the buffered data into windows and crossfading from one window to the next.
  // For speeds greater than 1.0f, FillBuffer() "squish" the windows, dropping
  // some data in between windows to meet the sped-up playback. For speeds less
  // than 1.0f, FillBuffer() will "stretch" the window by copying and
  // overlapping data at the window boundaries, crossfading in between.
  //
  // Returns the number of bytes copied into |dest|.
  // May request more reads via |request_read_cb_| before returning.
  uint32 FillBuffer(uint8* dest, uint32 length);

  // Clears |audio_buffer_|.
  void FlushBuffers();

  // Returns the time of the next byte in our data or kNoTimestamp() if current
  // time is unknown.
  base::TimeDelta GetTime();

  // Enqueues a buffer. It is called from the owner of the algorithm after a
  // read completes.
  void EnqueueBuffer(Buffer* buffer_in);

  float playback_rate() const { return playback_rate_; }
  void SetPlaybackRate(float new_rate);

  // Returns whether |audio_buffer_| is empty.
  bool IsQueueEmpty();

  // Returns true if |audio_buffer_| is at or exceeds capacity.
  bool IsQueueFull();

  // Returns the capacity of |audio_buffer_|.
  uint32 QueueCapacity();

  // Increase the capacity of |audio_buffer_| if possible.
  void IncreaseQueueCapacity();

  // Returns the number of bytes left in |audio_buffer_|, which may be larger
  // than QueueCapacity() in the event that a read callback delivered more data
  // than |audio_buffer_| was intending to hold.
  uint32 bytes_buffered() { return audio_buffer_.forward_bytes(); }

  uint32 window_size() { return window_size_; }

 private:
  // Advances |audio_buffer_|'s internal pointer by |bytes|.
  void AdvanceBufferPosition(uint32 bytes);

  // Tries to copy |bytes| bytes from |audio_buffer_| to |dest|.
  // Returns the number of bytes successfully copied.
  uint32 CopyFromAudioBuffer(uint8* dest, uint32 bytes);

  // Aligns |value| to a channel and sample boundary.
  void AlignToSampleBoundary(uint32* value);

  // Attempts to write |length| bytes of muted audio into |dest|.
  uint32 MuteBuffer(uint8* dest, uint32 length);

  // Number of channels in audio stream.
  int channels_;

  // Sample rate of audio stream.
  int samples_per_second_;

  // Byte depth of audio.
  int bytes_per_channel_;

  // Used by algorithm to scale output.
  float playback_rate_;

  // Used to request more data.
  base::Closure request_read_cb_;

  // Buffered audio data.
  SeekableBuffer audio_buffer_;

  // Length for crossfade in bytes.
  uint32 crossfade_size_;

  // Window size, in bytes (calculated from audio properties).
  uint32 window_size_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererAlgorithmBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_BASE_H_
