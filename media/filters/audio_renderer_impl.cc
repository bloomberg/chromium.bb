// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_impl.h"

#include <math.h>

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_splicer.h"
#include "media/base/bind_to_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/filters/audio_decoder_selector.h"
#include "media/filters/decrypting_demuxer_stream.h"

namespace media {

namespace {

enum AudioRendererEvent {
  INITIALIZED,
  RENDER_ERROR,
  MAX_EVENTS
};

void HistogramRendererEvent(AudioRendererEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Media.AudioRendererEvents", event, MAX_EVENTS);
}

}  // namespace

AudioRendererImpl::AudioRendererImpl(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    media::AudioRendererSink* sink,
    ScopedVector<AudioDecoder> decoders,
    const SetDecryptorReadyCB& set_decryptor_ready_cb)
    : message_loop_(message_loop),
      weak_factory_(this),
      sink_(sink),
      decoder_selector_(new AudioDecoderSelector(
          message_loop, decoders.Pass(), set_decryptor_ready_cb)),
      now_cb_(base::Bind(&base::TimeTicks::Now)),
      state_(kUninitialized),
      sink_playing_(false),
      pending_read_(false),
      received_end_of_stream_(false),
      rendered_end_of_stream_(false),
      audio_time_buffered_(kNoTimestamp()),
      current_time_(kNoTimestamp()),
      underflow_disabled_(false),
      preroll_aborted_(false) {
}

AudioRendererImpl::~AudioRendererImpl() {
  // Stop() should have been called and |algorithm_| should have been destroyed.
  DCHECK(state_ == kUninitialized || state_ == kStopped);
  DCHECK(!algorithm_.get());
}

void AudioRendererImpl::Play(const base::Closure& callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPaused);
  ChangeState_Locked(kPlaying);
  callback.Run();
  earliest_end_time_ = now_cb_.Run();

  if (algorithm_->playback_rate() != 0)
    DoPlay_Locked();
  else
    DCHECK(!sink_playing_);
}

void AudioRendererImpl::DoPlay_Locked() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  lock_.AssertAcquired();
  earliest_end_time_ = now_cb_.Run();

  if ((state_ == kPlaying || state_ == kRebuffering || state_ == kUnderflow) &&
      !sink_playing_) {
    {
      base::AutoUnlock auto_unlock(lock_);
      sink_->Play();
    }

    sink_playing_ = true;
  }
}

void AudioRendererImpl::Pause(const base::Closure& callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == kPlaying || state_ == kUnderflow ||
         state_ == kRebuffering) << "state_ == " << state_;
  ChangeState_Locked(kPaused);

  DoPause_Locked();

  callback.Run();
}

void AudioRendererImpl::DoPause_Locked() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (sink_playing_) {
    {
      base::AutoUnlock auto_unlock(lock_);
      sink_->Pause();
    }
    sink_playing_ = false;
  }
}

void AudioRendererImpl::Flush(const base::Closure& callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPaused);
  DCHECK(flush_cb_.is_null());

  flush_cb_ = callback;

  if (pending_read_) {
    ChangeState_Locked(kFlushing);
    return;
  }

  DoFlush_Locked();
}

void AudioRendererImpl::DoFlush_Locked() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  DCHECK(!pending_read_);
  DCHECK_EQ(state_, kPaused);

  if (decrypting_demuxer_stream_) {
    decrypting_demuxer_stream_->Reset(BindToCurrentLoop(
        base::Bind(&AudioRendererImpl::ResetDecoder, weak_this_)));
    return;
  }

  ResetDecoder();
}

void AudioRendererImpl::ResetDecoder() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  decoder_->Reset(BindToCurrentLoop(
      base::Bind(&AudioRendererImpl::ResetDecoderDone, weak_this_)));
}

void AudioRendererImpl::ResetDecoderDone() {
  base::AutoLock auto_lock(lock_);
  if (state_ == kStopped)
    return;

  DCHECK_EQ(state_, kPaused);
  DCHECK(!flush_cb_.is_null());

  audio_time_buffered_ = kNoTimestamp();
  current_time_ = kNoTimestamp();
  received_end_of_stream_ = false;
  rendered_end_of_stream_ = false;
  preroll_aborted_ = false;

  earliest_end_time_ = now_cb_.Run();
  splicer_->Reset();
  algorithm_->FlushBuffers();

  base::ResetAndReturn(&flush_cb_).Run();
}

void AudioRendererImpl::Stop(const base::Closure& callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!callback.is_null());

  // TODO(scherkus): Consider invalidating |weak_factory_| and replacing
  // task-running guards that check |state_| with DCHECK().

  if (sink_) {
    sink_->Stop();
    sink_ = NULL;
  }

  {
    base::AutoLock auto_lock(lock_);
    ChangeState_Locked(kStopped);
    algorithm_.reset(NULL);
    init_cb_.Reset();
    underflow_cb_.Reset();
    time_cb_.Reset();
    flush_cb_.Reset();
  }

  callback.Run();
}

void AudioRendererImpl::Preroll(base::TimeDelta time,
                                const PipelineStatusCB& cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK(!sink_playing_);
  DCHECK_EQ(state_, kPaused);
  DCHECK(!pending_read_) << "Pending read must complete before seeking";
  DCHECK(preroll_cb_.is_null());

  ChangeState_Locked(kPrerolling);
  preroll_cb_ = cb;
  preroll_timestamp_ = time;

  AttemptRead_Locked();
}

void AudioRendererImpl::Initialize(DemuxerStream* stream,
                                   const PipelineStatusCB& init_cb,
                                   const StatisticsCB& statistics_cb,
                                   const base::Closure& underflow_cb,
                                   const TimeCB& time_cb,
                                   const base::Closure& ended_cb,
                                   const base::Closure& disabled_cb,
                                   const PipelineStatusCB& error_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(stream);
  DCHECK_EQ(stream->type(), DemuxerStream::AUDIO);
  DCHECK(!init_cb.is_null());
  DCHECK(!statistics_cb.is_null());
  DCHECK(!underflow_cb.is_null());
  DCHECK(!time_cb.is_null());
  DCHECK(!ended_cb.is_null());
  DCHECK(!disabled_cb.is_null());
  DCHECK(!error_cb.is_null());
  DCHECK_EQ(kUninitialized, state_);
  DCHECK(sink_);

  weak_this_ = weak_factory_.GetWeakPtr();
  init_cb_ = init_cb;
  statistics_cb_ = statistics_cb;
  underflow_cb_ = underflow_cb;
  time_cb_ = time_cb;
  ended_cb_ = ended_cb;
  disabled_cb_ = disabled_cb;
  error_cb_ = error_cb;

  decoder_selector_->SelectAudioDecoder(
      stream,
      statistics_cb,
      base::Bind(&AudioRendererImpl::OnDecoderSelected, weak_this_));
}

void AudioRendererImpl::OnDecoderSelected(
    scoped_ptr<AudioDecoder> decoder,
    scoped_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  scoped_ptr<AudioDecoderSelector> deleter(decoder_selector_.Pass());

  if (state_ == kStopped) {
    DCHECK(!sink_);
    return;
  }

  if (!decoder) {
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  decoder_ = decoder.Pass();
  decrypting_demuxer_stream_ = decrypting_demuxer_stream.Pass();

  int sample_rate = decoder_->samples_per_second();

  // The actual buffer size is controlled via the size of the AudioBus provided
  // to Render(), so just choose something reasonable here for looks.
  int buffer_size = decoder_->samples_per_second() / 100;
  audio_parameters_ = AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, decoder_->channel_layout(),
      sample_rate, decoder_->bits_per_channel(), buffer_size);
  if (!audio_parameters_.IsValid()) {
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  splicer_.reset(new AudioSplicer(sample_rate));

  // We're all good! Continue initializing the rest of the audio renderer based
  // on the decoder format.
  algorithm_.reset(new AudioRendererAlgorithm());
  algorithm_->Initialize(0, audio_parameters_);

  ChangeState_Locked(kPaused);

  HistogramRendererEvent(INITIALIZED);

  {
    base::AutoUnlock auto_unlock(lock_);
    sink_->Initialize(audio_parameters_, weak_this_.get());
    sink_->Start();

    // Some sinks play on start...
    sink_->Pause();
  }

  DCHECK(!sink_playing_);

  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

void AudioRendererImpl::ResumeAfterUnderflow() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  if (state_ == kUnderflow) {
    // The "!preroll_aborted_" is a hack. If preroll is aborted, then we
    // shouldn't even reach the kUnderflow state to begin with. But for now
    // we're just making sure that the audio buffer capacity (i.e. the
    // number of bytes that need to be buffered for preroll to complete)
    // does not increase due to an aborted preroll.
    // TODO(vrk): Fix this bug correctly! (crbug.com/151352)
    if (!preroll_aborted_)
      algorithm_->IncreaseQueueCapacity();

    ChangeState_Locked(kRebuffering);
  }
}

void AudioRendererImpl::SetVolume(float volume) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(sink_);
  sink_->SetVolume(volume);
}

void AudioRendererImpl::DecodedAudioReady(
    AudioDecoder::Status status,
    const scoped_refptr<AudioBuffer>& buffer) {
  DVLOG(1) << __FUNCTION__ << "(" << status << ")";
  DCHECK(message_loop_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK(state_ != kUninitialized);

  CHECK(pending_read_);
  pending_read_ = false;

  if (status == AudioDecoder::kAborted) {
    HandleAbortedReadOrDecodeError(false);
    return;
  }

  if (status == AudioDecoder::kDecodeError) {
    HandleAbortedReadOrDecodeError(true);
    return;
  }

  DCHECK_EQ(status, AudioDecoder::kOk);
  DCHECK(buffer.get());

  if (state_ == kFlushing) {
    ChangeState_Locked(kPaused);
    DoFlush_Locked();
    return;
  }

  if (!splicer_->AddInput(buffer)) {
    HandleAbortedReadOrDecodeError(true);
    return;
  }

  if (!splicer_->HasNextBuffer()) {
    AttemptRead_Locked();
    return;
  }

  bool need_another_buffer = false;
  while (splicer_->HasNextBuffer())
    need_another_buffer = HandleSplicerBuffer(splicer_->GetNextBuffer());

  if (!need_another_buffer && !CanRead_Locked())
    return;

  AttemptRead_Locked();
}

bool AudioRendererImpl::HandleSplicerBuffer(
    const scoped_refptr<AudioBuffer>& buffer) {
  if (buffer->end_of_stream()) {
    received_end_of_stream_ = true;

    // Transition to kPlaying if we are currently handling an underflow since
    // no more data will be arriving.
    if (state_ == kUnderflow || state_ == kRebuffering)
      ChangeState_Locked(kPlaying);
  } else {
    if (state_ == kPrerolling) {
      if (IsBeforePrerollTime(buffer))
        return true;

      // Trim off any additional time before the preroll timestamp.
      const base::TimeDelta trim_time =
          preroll_timestamp_ - buffer->timestamp();
      if (trim_time > base::TimeDelta()) {
        buffer->TrimStart(buffer->frame_count() *
                          (static_cast<double>(trim_time.InMicroseconds()) /
                           buffer->duration().InMicroseconds()));
      }
      // If the entire buffer was trimmed, request a new one.
      if (!buffer->frame_count())
        return true;
    }

    if (state_ != kUninitialized && state_ != kStopped)
      algorithm_->EnqueueBuffer(buffer);
  }

  switch (state_) {
    case kUninitialized:
    case kFlushing:
      NOTREACHED();
      return false;

    case kPaused:
      DCHECK(!pending_read_);
      return false;

    case kPrerolling:
      if (!buffer->end_of_stream() && !algorithm_->IsQueueFull())
        return true;
      ChangeState_Locked(kPaused);
      base::ResetAndReturn(&preroll_cb_).Run(PIPELINE_OK);
      return false;

    case kPlaying:
    case kUnderflow:
      return false;

    case kRebuffering:
      if (!algorithm_->IsQueueFull())
        return true;
      ChangeState_Locked(kPlaying);
      return false;

    case kStopped:
      return false;
  }
  return false;
}

void AudioRendererImpl::AttemptRead() {
  base::AutoLock auto_lock(lock_);
  AttemptRead_Locked();
}

void AudioRendererImpl::AttemptRead_Locked() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (!CanRead_Locked())
    return;

  pending_read_ = true;
  decoder_->Read(base::Bind(&AudioRendererImpl::DecodedAudioReady, weak_this_));
}

bool AudioRendererImpl::CanRead_Locked() {
  lock_.AssertAcquired();

  switch (state_) {
    case kUninitialized:
    case kPaused:
    case kFlushing:
    case kStopped:
      return false;

    case kPrerolling:
    case kPlaying:
    case kUnderflow:
    case kRebuffering:
      break;
  }

  return !pending_read_ && !received_end_of_stream_ &&
      !algorithm_->IsQueueFull();
}

void AudioRendererImpl::SetPlaybackRate(float playback_rate) {
  DVLOG(1) << __FUNCTION__ << "(" << playback_rate << ")";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_GE(playback_rate, 0);
  DCHECK(sink_);

  base::AutoLock auto_lock(lock_);

  // We have two cases here:
  // Play: current_playback_rate == 0 && playback_rate != 0
  // Pause: current_playback_rate != 0 && playback_rate == 0
  float current_playback_rate = algorithm_->playback_rate();
  if (current_playback_rate == 0 && playback_rate != 0)
    DoPlay_Locked();
  else if (current_playback_rate != 0 && playback_rate == 0)
    DoPause_Locked();

  algorithm_->SetPlaybackRate(playback_rate);
}

bool AudioRendererImpl::IsBeforePrerollTime(
    const scoped_refptr<AudioBuffer>& buffer) {
  DCHECK_EQ(state_, kPrerolling);
  return buffer && !buffer->end_of_stream() &&
         (buffer->timestamp() + buffer->duration()) < preroll_timestamp_;
}

int AudioRendererImpl::Render(AudioBus* audio_bus,
                              int audio_delay_milliseconds) {
  const int requested_frames = audio_bus->frames();
  base::TimeDelta current_time = kNoTimestamp();
  base::TimeDelta max_time = kNoTimestamp();
  base::TimeDelta playback_delay = base::TimeDelta::FromMilliseconds(
      audio_delay_milliseconds);

  int frames_written = 0;
  base::Closure underflow_cb;
  {
    base::AutoLock auto_lock(lock_);

    // Ensure Stop() hasn't destroyed our |algorithm_| on the pipeline thread.
    if (!algorithm_)
      return 0;

    float playback_rate = algorithm_->playback_rate();
    if (playback_rate == 0)
      return 0;

    // Mute audio by returning 0 when not playing.
    if (state_ != kPlaying)
      return 0;

    // We use the following conditions to determine end of playback:
    //   1) Algorithm can not fill the audio callback buffer
    //   2) We received an end of stream buffer
    //   3) We haven't already signalled that we've ended
    //   4) Our estimated earliest end time has expired
    //
    // TODO(enal): we should replace (4) with a check that the browser has no
    // more audio data or at least use a delayed callback.
    //
    // We use the following conditions to determine underflow:
    //   1) Algorithm can not fill the audio callback buffer
    //   2) We have NOT received an end of stream buffer
    //   3) We are in the kPlaying state
    //
    // Otherwise the buffer has data we can send to the device.
    frames_written = algorithm_->FillBuffer(audio_bus, requested_frames);
    if (frames_written == 0) {
      const base::TimeTicks now = now_cb_.Run();

      if (received_end_of_stream_ && !rendered_end_of_stream_ &&
          now >= earliest_end_time_) {
        rendered_end_of_stream_ = true;
        ended_cb_.Run();
      } else if (!received_end_of_stream_ && state_ == kPlaying &&
                 !underflow_disabled_) {
        ChangeState_Locked(kUnderflow);
        underflow_cb = underflow_cb_;
      } else {
        // We can't write any data this cycle. For example, we may have
        // sent all available data to the audio device while not reaching
        // |earliest_end_time_|.
      }
    }

    if (CanRead_Locked()) {
      message_loop_->PostTask(FROM_HERE, base::Bind(
          &AudioRendererImpl::AttemptRead, weak_this_));
    }

    // The |audio_time_buffered_| is the ending timestamp of the last frame
    // buffered at the audio device. |playback_delay| is the amount of time
    // buffered at the audio device. The current time can be computed by their
    // difference.
    if (audio_time_buffered_ != kNoTimestamp()) {
      // Adjust the delay according to playback rate.
      base::TimeDelta adjusted_playback_delay =
          base::TimeDelta::FromMicroseconds(ceil(
              playback_delay.InMicroseconds() * playback_rate));

      base::TimeDelta previous_time = current_time_;
      current_time_ = audio_time_buffered_ - adjusted_playback_delay;

      // Time can change in one of two ways:
      //   1) The time of the audio data at the audio device changed, or
      //   2) The playback delay value has changed
      //
      // We only want to set |current_time| (and thus execute |time_cb_|) if
      // time has progressed and we haven't signaled end of stream yet.
      //
      // Why? The current latency of the system results in getting the last call
      // to FillBuffer() later than we'd like, which delays firing the 'ended'
      // event, which delays the looping/trigging performance of short sound
      // effects.
      //
      // TODO(scherkus): revisit this and switch back to relying on playback
      // delay after we've revamped our audio IPC subsystem.
      if (current_time_ > previous_time && !rendered_end_of_stream_) {
        current_time = current_time_;
      }
    }

    // The call to FillBuffer() on |algorithm_| has increased the amount of
    // buffered audio data. Update the new amount of time buffered.
    max_time = algorithm_->GetTime();
    audio_time_buffered_ = max_time;

    if (frames_written > 0) {
      UpdateEarliestEndTime_Locked(
          frames_written, playback_delay, now_cb_.Run());
    }
  }

  if (current_time != kNoTimestamp() && max_time != kNoTimestamp())
    time_cb_.Run(current_time, max_time);

  if (!underflow_cb.is_null())
    underflow_cb.Run();

  DCHECK_LE(frames_written, requested_frames);
  return frames_written;
}

void AudioRendererImpl::UpdateEarliestEndTime_Locked(
    int frames_filled, const base::TimeDelta& playback_delay,
    const base::TimeTicks& time_now) {
  DCHECK_GT(frames_filled, 0);

  base::TimeDelta predicted_play_time = base::TimeDelta::FromMicroseconds(
      static_cast<float>(frames_filled) * base::Time::kMicrosecondsPerSecond /
      audio_parameters_.sample_rate());

  lock_.AssertAcquired();
  earliest_end_time_ = std::max(
      earliest_end_time_, time_now + playback_delay + predicted_play_time);
}

void AudioRendererImpl::OnRenderError() {
  HistogramRendererEvent(RENDER_ERROR);
  disabled_cb_.Run();
}

void AudioRendererImpl::DisableUnderflowForTesting() {
  underflow_disabled_ = true;
}

void AudioRendererImpl::HandleAbortedReadOrDecodeError(bool is_decode_error) {
  lock_.AssertAcquired();

  PipelineStatus status = is_decode_error ? PIPELINE_ERROR_DECODE : PIPELINE_OK;
  switch (state_) {
    case kUninitialized:
      NOTREACHED();
      return;
    case kPaused:
      if (status != PIPELINE_OK)
        error_cb_.Run(status);
      return;
    case kFlushing:
      ChangeState_Locked(kPaused);

      if (status == PIPELINE_OK) {
        DoFlush_Locked();
        return;
      }

      error_cb_.Run(status);
      base::ResetAndReturn(&flush_cb_).Run();
      return;
    case kPrerolling:
      // This is a signal for abort if it's not an error.
      preroll_aborted_ = !is_decode_error;
      ChangeState_Locked(kPaused);
      base::ResetAndReturn(&preroll_cb_).Run(status);
      return;
    case kPlaying:
    case kUnderflow:
    case kRebuffering:
    case kStopped:
      if (status != PIPELINE_OK)
        error_cb_.Run(status);
      return;
  }
}

void AudioRendererImpl::ChangeState_Locked(State new_state) {
  DVLOG(1) << __FUNCTION__ << " : " << state_ << " -> " << new_state;
  lock_.AssertAcquired();
  state_ = new_state;
}

}  // namespace media
