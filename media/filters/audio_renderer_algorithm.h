// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioRendererAlgorithm buffers and transforms audio data. The owner of
// this object provides audio data to the object through EnqueueBuffer() and
// requests data from the buffer via FillBuffer(). The owner also sets the
// playback rate, and the AudioRendererAlgorithm will stretch or compress the
// buffered audio as necessary to match the playback rate when fulfilling
// FillBuffer() requests.
//
// This class is *not* thread-safe. Calls to enqueue and retrieve data must be
// locked if called from multiple threads.
//
// AudioRendererAlgorithm uses a simple pitch-preservation algorithm to
// stretch and compress audio data to meet playback speeds less than and
// greater than the natural playback of the audio stream.
//
// Audio at very low or very high playback rates are muted to preserve quality.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_buffer_queue.h"

namespace media {

class AudioBus;

class MEDIA_EXPORT AudioRendererAlgorithm {
 public:
  AudioRendererAlgorithm();
  ~AudioRendererAlgorithm();

  // Initializes this object with information about the audio stream.
  void Initialize(float initial_playback_rate, const AudioParameters& params);

  // Tries to fill |requested_frames| frames into |dest| with possibly scaled
  // data from our |audio_buffer_|. Data is scaled based on the playback rate,
  // using a variation of the Overlap-Add method to combine sample windows.
  //
  // Data from |audio_buffer_| is consumed in proportion to the playback rate.
  //
  // Returns the number of frames copied into |dest|. May request more reads via
  // |request_read_cb_| before returning.
  int FillBuffer(AudioBus* dest, int requested_frames);

  // Clears |audio_buffer_|.
  void FlushBuffers();

  // Returns the time of the next byte in our data or kNoTimestamp() if current
  // time is unknown.
  base::TimeDelta GetTime();

  // Enqueues a buffer. It is called from the owner of the algorithm after a
  // read completes.
  void EnqueueBuffer(const scoped_refptr<AudioBuffer>& buffer_in);

  float playback_rate() const { return playback_rate_; }
  void SetPlaybackRate(float new_rate);

  // Returns true if |audio_buffer_| is at or exceeds capacity.
  bool IsQueueFull();

  // Returns the capacity of |audio_buffer_| in frames.
  int QueueCapacity() const { return capacity_; }

  // Increase the capacity of |audio_buffer_| if possible.
  void IncreaseQueueCapacity();

  // Returns the number of frames left in |audio_buffer_|, which may be larger
  // than QueueCapacity() in the event that EnqueueBuffer() delivered more data
  // than |audio_buffer_| was intending to hold.
  int frames_buffered() { return audio_buffer_.frames(); }

  // Returns the samples per second for this audio stream.
  int samples_per_second() { return samples_per_second_; }

  // Is the sound currently muted?
  bool is_muted() { return muted_; }

 private:
  // Fills |dest| with up to |requested_frames| frames of audio data at faster
  // than normal speed. Returns the number of frames inserted into |dest|. If
  // not enough data available, returns 0.
  //
  // When the audio playback is > 1.0, we use a variant of Overlap-Add to squish
  // audio output while preserving pitch. Essentially, we play a bit of audio
  // data at normal speed, then we "fast forward" by dropping the next bit of
  // audio data, and then we stich the pieces together by crossfading from one
  // audio chunk to the next.
  int OutputFasterPlayback(AudioBus* dest,
                           int dest_offset,
                           int requested_frames,
                           int input_step,
                           int output_step);

  // Fills |dest| with up to |requested_frames| frames of audio data at slower
  // than normal speed. Returns the number of frames inserted into |dest|. If
  // not enough data available, returns 0.
  //
  // When the audio playback is < 1.0, we use a variant of Overlap-Add to
  // stretch audio output while preserving pitch. This works by outputting a
  // segment of audio data at normal speed. The next audio segment then starts
  // by repeating some of the audio data from the previous audio segment.
  // Segments are stiched together by crossfading from one audio chunk to the
  // next.
  int OutputSlowerPlayback(AudioBus* dest,
                           int dest_offset,
                           int requested_frames,
                           int input_step,
                           int output_step);

  // Resets the window state to the start of a new window.
  void ResetWindow();

  // Does a linear crossfade from |intro| into |outtro| for one frame.
  void CrossfadeFrame(AudioBus* intro,
                      int intro_offset,
                      AudioBus* outtro,
                      int outtro_offset,
                      int fade_offset);

  // Number of channels in audio stream.
  int channels_;

  // Sample rate of audio stream.
  int samples_per_second_;

  // Used by algorithm to scale output.
  float playback_rate_;

  // Buffered audio data.
  AudioBufferQueue audio_buffer_;

  // Length for crossfade in frames.
  int frames_in_crossfade_;

  // The current location in the audio window, between 0 and |window_size_|.
  // When |index_into_window_| reaches |window_size_|, the window resets.
  // Indexed by frame.
  int index_into_window_;

  // The frame number in the crossfade.
  int crossfade_frame_number_;

  // True if the audio should be muted.
  bool muted_;

  // If muted, keep track of partial frames that should have been skipped over.
  double muted_partial_frame_;

  // Temporary buffer to hold crossfade data.
  scoped_ptr<AudioBus> crossfade_buffer_;

  // Window size, in frames (calculated from audio properties).
  int window_size_;

  // How many frames to have in the queue before we report the queue is full.
  int capacity_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererAlgorithm);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_H_
