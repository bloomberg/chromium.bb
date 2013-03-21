// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Audio rendering unit utilizing an AudioRendererSink to output data.
//
// This class lives inside three threads during it's lifetime, namely:
// 1. Render thread
//    Where the object is created.
// 2. Media thread (provided via constructor)
//    All AudioDecoder methods are called on this thread.
// 3. Audio thread created by the AudioRendererSink.
//    Render() is called here where audio data is decoded into raw PCM data.
//
// AudioRendererImpl talks to an AudioRendererAlgorithm that takes care of
// queueing audio data and stretching/shrinking audio data when playback rate !=
// 1.0 or 0.0.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_

#include <deque>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_renderer.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/decryptor.h"
#include "media/filters/audio_renderer_algorithm.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class AudioDecoderSelector;
class AudioSplicer;
class DecryptingDemuxerStream;

class MEDIA_EXPORT AudioRendererImpl
    : public AudioRenderer,
      NON_EXPORTED_BASE(public AudioRendererSink::RenderCallback) {
 public:
  // |message_loop| is the thread on which AudioRendererImpl will execute.
  //
  // |sink| is used as the destination for the rendered audio.
  //
  // |decoders| contains the AudioDecoders to use when initializing.
  //
  // |set_decryptor_ready_cb| is fired when the audio decryptor is available
  // (only applicable if the stream is encrypted and we have a decryptor).
  AudioRendererImpl(const scoped_refptr<base::MessageLoopProxy>& message_loop,
                    AudioRendererSink* sink,
                    ScopedVector<AudioDecoder> decoders,
                    const SetDecryptorReadyCB& set_decryptor_ready_cb);
  virtual ~AudioRendererImpl();

  // AudioRenderer implementation.
  virtual void Initialize(const scoped_refptr<DemuxerStream>& stream,
                          const PipelineStatusCB& init_cb,
                          const StatisticsCB& statistics_cb,
                          const base::Closure& underflow_cb,
                          const TimeCB& time_cb,
                          const base::Closure& ended_cb,
                          const base::Closure& disabled_cb,
                          const PipelineStatusCB& error_cb) OVERRIDE;
  virtual void Play(const base::Closure& callback) OVERRIDE;
  virtual void Pause(const base::Closure& callback) OVERRIDE;
  virtual void Flush(const base::Closure& callback) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void SetPlaybackRate(float rate) OVERRIDE;
  virtual void Preroll(base::TimeDelta time,
                       const PipelineStatusCB& cb) OVERRIDE;
  virtual void ResumeAfterUnderflow(bool buffer_more_audio) OVERRIDE;
  virtual void SetVolume(float volume) OVERRIDE;

  // Disables underflow support.  When used, |state_| will never transition to
  // kUnderflow resulting in Render calls that underflow returning 0 frames
  // instead of some number of silence frames.  Must be called prior to
  // Initialize().
  void DisableUnderflowForTesting();

  // Allows injection of a custom time callback for non-realtime testing.
  typedef base::Callback<base::Time()> NowCB;
  void set_now_cb_for_testing(const NowCB& now_cb) {
    now_cb_ = now_cb;
  }

 private:
  friend class AudioRendererImplTest;

  // Callback from the audio decoder delivering decoded audio samples.
  void DecodedAudioReady(AudioDecoder::Status status,
                         const scoped_refptr<DataBuffer>& buffer);

  // Handles buffers that come out of |splicer_|.
  // Returns true if more buffers are needed.
  bool HandleSplicerBuffer(const scoped_refptr<DataBuffer>& buffer);

  // Helper functions for AudioDecoder::Status values passed to
  // DecodedAudioReady().
  void HandleAbortedReadOrDecodeError(bool is_decode_error);

  // Fills the given buffer with audio data by delegating to its |algorithm_|.
  // FillBuffer() also takes care of updating the clock. Returns the number of
  // frames copied into |dest|, which may be less than or equal to
  // |requested_frames|.
  //
  // If this method returns fewer frames than |requested_frames|, it could
  // be a sign that the pipeline is stalled or unable to stream the data fast
  // enough.  In such scenarios, the callee should zero out unused portions
  // of their buffer to playback silence.
  //
  // FillBuffer() updates the pipeline's playback timestamp. If FillBuffer() is
  // not called at the same rate as audio samples are played, then the reported
  // timestamp in the pipeline will be ahead of the actual audio playback. In
  // this case |playback_delay| should be used to indicate when in the future
  // should the filled buffer be played.
  //
  // Safe to call on any thread.
  uint32 FillBuffer(uint8* dest,
                    uint32 requested_frames,
                    int audio_delay_milliseconds);

  // Estimate earliest time when current buffer can stop playing.
  void UpdateEarliestEndTime_Locked(int frames_filled,
                                    base::TimeDelta playback_delay,
                                    base::Time time_now);

  void DoPlay();
  void DoPause();

  // AudioRendererSink::RenderCallback implementation.
  //
  // NOTE: These are called on the audio callback thread!
  virtual int Render(AudioBus* audio_bus,
                     int audio_delay_milliseconds) OVERRIDE;
  virtual void OnRenderError() OVERRIDE;

  // Helper methods that schedule an asynchronous read from the decoder as long
  // as there isn't a pending read.
  //
  // Must be called on |message_loop_|.
  void AttemptRead();
  void AttemptRead_Locked();
  bool CanRead_Locked();

  // Returns true if the data in the buffer is all before
  // |preroll_timestamp_|. This can only return true while
  // in the kPrerolling state.
  bool IsBeforePrerollTime(const scoped_refptr<DataBuffer>& buffer);

  // Called when |decoder_selector_| has selected |decoder| or is null if no
  // decoder could be selected.
  //
  // |decrypting_demuxer_stream| is non-null if a DecryptingDemuxerStream was
  // created to help decrypt the encrypted stream.
  void OnDecoderSelected(
      scoped_ptr<AudioDecoder> decoder,
      const scoped_refptr<DecryptingDemuxerStream>& decrypting_demuxer_stream);

  void ResetDecoder(const base::Closure& callback);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::WeakPtrFactory<AudioRendererImpl> weak_factory_;
  base::WeakPtr<AudioRendererImpl> weak_this_;

  scoped_ptr<AudioSplicer> splicer_;

  // The sink (destination) for rendered audio. |sink_| must only be accessed
  // on |message_loop_|. |sink_| must never be called under |lock_| or else we
  // may deadlock between |message_loop_| and the audio callback thread.
  scoped_refptr<media::AudioRendererSink> sink_;

  scoped_ptr<AudioDecoderSelector> decoder_selector_;

  // These two will be set by AudioDecoderSelector::SelectAudioDecoder().
  scoped_ptr<AudioDecoder> decoder_;
  scoped_refptr<DecryptingDemuxerStream> decrypting_demuxer_stream_;

  // AudioParameters constructed during Initialize() based on |decoder_|.
  AudioParameters audio_parameters_;

  // Callbacks provided during Initialize().
  PipelineStatusCB init_cb_;
  StatisticsCB statistics_cb_;
  base::Closure underflow_cb_;
  TimeCB time_cb_;
  base::Closure ended_cb_;
  base::Closure disabled_cb_;
  PipelineStatusCB error_cb_;

  // Callback provided to Pause().
  base::Closure pause_cb_;

  // Callback provided to Preroll().
  PipelineStatusCB preroll_cb_;

  // Typically calls base::Time::Now() but can be overridden by a test.
  NowCB now_cb_;

  // After Initialize() has completed, all variables below must be accessed
  // under |lock_|. ------------------------------------------------------------
  base::Lock lock_;

  // Algorithm for scaling audio.
  scoped_ptr<AudioRendererAlgorithm> algorithm_;

  // Simple state tracking variable.
  enum State {
    kUninitialized,
    kPaused,
    kPrerolling,
    kPlaying,
    kStopped,
    kUnderflow,
    kRebuffering,
  };
  State state_;

  // Keep track of our outstanding read to |decoder_|.
  bool pending_read_;

  // Keeps track of whether we received and rendered the end of stream buffer.
  bool received_end_of_stream_;
  bool rendered_end_of_stream_;

  // The timestamp of the last frame (i.e. furthest in the future) buffered as
  // well as the current time that takes current playback delay into account.
  base::TimeDelta audio_time_buffered_;
  base::TimeDelta current_time_;

  base::TimeDelta preroll_timestamp_;

  // We're supposed to know amount of audio data OS or hardware buffered, but
  // that is not always so -- on my Linux box
  // AudioBuffersState::hardware_delay_bytes never reaches 0.
  //
  // As a result we cannot use it to find when stream ends. If we just ignore
  // buffered data we will notify host that stream ended before it is actually
  // did so, I've seen it done ~140ms too early when playing ~150ms file.
  //
  // Instead of trying to invent OS-specific solution for each and every OS we
  // are supporting, use simple workaround: every time we fill the buffer we
  // remember when it should stop playing, and do not assume that buffer is
  // empty till that time. Workaround is not bulletproof, as we don't exactly
  // know when that particular data would start playing, but it is much better
  // than nothing.
  base::Time earliest_end_time_;
  size_t total_frames_filled_;

  bool underflow_disabled_;

  // True if the renderer receives a buffer with kAborted status during preroll,
  // false otherwise. This flag is cleared on the next Preroll() call.
  bool preroll_aborted_;

  // End variables which must be accessed under |lock_|. ----------------------

  // Variables used only on the audio thread. ---------------------------------
  int actual_frames_per_buffer_;
  scoped_array<uint8> audio_buffer_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_
