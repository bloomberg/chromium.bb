// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioRendererBase takes care of the tricky queuing work and provides simple
// methods for subclasses to peek and poke at audio data.  In addition to
// AudioRenderer interface methods this classes doesn't implement, subclasses
// must also implement the following methods:
//   OnInitialized
//   OnStop
//
// The general assumption is that subclasses start a callback-based audio thread
// which needs to be filled with decoded audio data.  AudioDecoderBase provides
// FillBuffer which handles filling the provided buffer, dequeuing items,
// scheduling additional reads and updating the clock.  In a sense,
// AudioRendererBase is the producer and the subclass is the consumer.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_BASE_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_BASE_H_

#include <deque>

#include "base/synchronization/lock.h"
#include "media/base/buffers.h"
#include "media/base/filters.h"
#include "media/filters/audio_renderer_algorithm_base.h"

namespace media {

class MEDIA_EXPORT AudioRendererBase : public AudioRenderer {
 public:
  AudioRendererBase();
  virtual ~AudioRendererBase();

  // Filter implementation.
  virtual void Play(FilterCallback* callback);
  virtual void Pause(FilterCallback* callback);
  virtual void Stop(FilterCallback* callback);

  virtual void Seek(base::TimeDelta time, const FilterStatusCB& cb);

  // AudioRenderer implementation.
  virtual void Initialize(AudioDecoder* decoder, FilterCallback* callback);
  virtual bool HasEnded();

 protected:
  // Subclasses should return true if they were able to initialize, false
  // otherwise.
  virtual bool OnInitialize(const AudioDecoderConfig& config) = 0;

  // Called by Stop().  Subclasses should perform any necessary cleanup during
  // this time, such as stopping any running threads.
  virtual void OnStop() = 0;

  // Called when a AudioDecoder completes decoding and decrements
  // |pending_reads_|.
  virtual void ConsumeAudioSamples(scoped_refptr<Buffer> buffer_in);

  // Fills the given buffer with audio data by delegating to its |algorithm_|.
  // FillBuffer() also takes care of updating the clock. Returns the number of
  // bytes copied into |dest|, which may be less than or equal to |len|.
  //
  // If this method is returns less bytes than |len| (including zero), it could
  // be a sign that the pipeline is stalled or unable to stream the data fast
  // enough.  In such scenarios, the callee should zero out unused portions
  // of their buffer to playback silence.
  //
  // FillBuffer() updates the pipeline's playback timestamp. If FillBuffer() is
  // not called at the same rate as audio samples are played, then the reported
  // timestamp in the pipeline will be ahead of the actual audio playback. In
  // this case |playback_delay| should be used to indicate when in the future
  // should the filled buffer be played. If FillBuffer() is called as the audio
  // hardware plays the buffer, then |playback_delay| should be zero.
  //
  // Safe to call on any thread.
  uint32 FillBuffer(uint8* dest,
                    uint32 len,
                    const base::TimeDelta& playback_delay,
                    bool buffers_empty);

  // Get/Set the playback rate of |algorithm_|.
  virtual void SetPlaybackRate(float playback_rate);
  virtual float GetPlaybackRate();

 private:
  // Helper method that schedules an asynchronous read from the decoder and
  // increments |pending_reads_|.
  //
  // Safe to call from any thread.
  void ScheduleRead_Locked();

  // Audio decoder.
  scoped_refptr<AudioDecoder> decoder_;

  // Algorithm for scaling audio.
  scoped_ptr<AudioRendererAlgorithmBase> algorithm_;

  base::Lock lock_;

  // Simple state tracking variable.
  enum State {
    kUninitialized,
    kPaused,
    kSeeking,
    kPlaying,
    kStopped,
    kError,
  };
  State state_;

  // Keeps track of whether we received and rendered the end of stream buffer.
  bool recieved_end_of_stream_;
  bool rendered_end_of_stream_;

  // Keeps track of our pending reads.  We *must* have no pending reads before
  // executing the pause callback, otherwise we breach the contract that all
  // filters are idling.
  //
  // We use size_t since we compare against std::deque::size().
  size_t pending_reads_;

  // Audio time at end of last call to FillBuffer().
  // TODO(ralphl): Update this value after seeking.
  base::TimeDelta last_fill_buffer_time_;

  // Filter callbacks.
  scoped_ptr<FilterCallback> pause_callback_;
  FilterStatusCB seek_cb_;

  base::TimeDelta seek_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_BASE_H_
