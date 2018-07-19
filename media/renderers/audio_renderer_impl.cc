// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/audio_renderer_impl.h"

#include <math.h>
#include <stddef.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/power_monitor/power_monitor.h"
#include "base/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_buffer_converter.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_parameters.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/channel_mixing_matrix.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_client.h"
#include "media/base/media_log.h"
#include "media/base/renderer_client.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/audio_clock.h"
#include "media/filters/decrypting_demuxer_stream.h"

#if defined(OS_WIN)
#include "media/base/media_switches.h"
#endif  // defined(OS_WIN)

namespace media {

AudioRendererImpl::AudioRendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    media::AudioRendererSink* sink,
    const CreateAudioDecodersCB& create_audio_decoders_cb,
    MediaLog* media_log)
    : task_runner_(task_runner),
      expecting_config_changes_(false),
      sink_(sink),
      media_log_(media_log),
      client_(nullptr),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      last_audio_memory_usage_(0),
      last_decoded_sample_rate_(0),
      last_decoded_channel_layout_(CHANNEL_LAYOUT_NONE),
      is_encrypted_(false),
      last_decoded_channels_(0),
      playback_rate_(0.0),
      state_(kUninitialized),
      create_audio_decoders_cb_(create_audio_decoders_cb),
      buffering_state_(BUFFERING_HAVE_NOTHING),
      rendering_(false),
      sink_playing_(false),
      pending_read_(false),
      received_end_of_stream_(false),
      rendered_end_of_stream_(false),
      is_suspending_(false),
      is_passthrough_(false),
      weak_factory_(this) {
  DCHECK(create_audio_decoders_cb_);
  // Tests may not have a power monitor.
  base::PowerMonitor* monitor = base::PowerMonitor::Get();
  if (!monitor)
    return;

  // PowerObserver's must be added and removed from the same thread, but we
  // won't remove the observer until we're destructed on |task_runner_| so we
  // must post it here if we're on the wrong thread.
  if (task_runner_->BelongsToCurrentThread()) {
    monitor->AddObserver(this);
  } else {
    // Safe to post this without a WeakPtr because this class must be destructed
    // on the same thread and construction has not completed yet.
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&base::PowerMonitor::AddObserver,
                                      base::Unretained(monitor), this));
  }

  // Do not add anything below this line since the above actions are only safe
  // as the last lines of the constructor.
}

AudioRendererImpl::~AudioRendererImpl() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (base::PowerMonitor::Get())
    base::PowerMonitor::Get()->RemoveObserver(this);

  // If Render() is in progress, this call will wait for Render() to finish.
  // After this call, the |sink_| will not call back into |this| anymore.
  sink_->Stop();

  // Trying to track down AudioClock crash, http://crbug.com/674856. If the sink
  // hasn't truly stopped above we will fail to acquire the lock. The sink must
  // be stopped to avoid destroying the AudioClock while its still being used.
  CHECK(lock_.Try());
  lock_.Release();

  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_ABORT);
}

void AudioRendererImpl::StartTicking() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);

  DCHECK(!rendering_);
  rendering_ = true;

  // Wait for an eventual call to SetPlaybackRate() to start rendering.
  if (playback_rate_ == 0) {
    DCHECK(!sink_playing_);
    return;
  }

  StartRendering_Locked();
}

void AudioRendererImpl::StartRendering_Locked() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPlaying);
  DCHECK(!sink_playing_);
  DCHECK_NE(playback_rate_, 0.0);
  lock_.AssertAcquired();

  sink_playing_ = true;

  base::AutoUnlock auto_unlock(lock_);
  sink_->Play();
}

void AudioRendererImpl::StopTicking() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);

  DCHECK(rendering_);
  rendering_ = false;

  // Rendering should have already been stopped with a zero playback rate.
  if (playback_rate_ == 0) {
    DCHECK(!sink_playing_);
    return;
  }

  StopRendering_Locked();
}

void AudioRendererImpl::StopRendering_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPlaying);
  DCHECK(sink_playing_);
  lock_.AssertAcquired();

  sink_playing_ = false;

  base::AutoUnlock auto_unlock(lock_);
  sink_->Pause();
  stop_rendering_time_ = last_render_time_;
}

void AudioRendererImpl::SetMediaTime(base::TimeDelta time) {
  DVLOG(1) << __func__ << "(" << time << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK(!rendering_);
  DCHECK_EQ(state_, kFlushed);

  start_timestamp_ = time;
  ended_timestamp_ = kInfiniteDuration;
  last_render_time_ = stop_rendering_time_ = base::TimeTicks();
  first_packet_timestamp_ = kNoTimestamp;
  audio_clock_.reset(new AudioClock(time, audio_parameters_.sample_rate()));
}

base::TimeDelta AudioRendererImpl::CurrentMediaTime() {
  base::AutoLock auto_lock(lock_);

  // Return the current time based on the known extents of the rendered audio
  // data plus an estimate based on the last time those values were calculated.
  base::TimeDelta current_media_time = audio_clock_->front_timestamp();
  if (!last_render_time_.is_null()) {
    current_media_time +=
        (tick_clock_->NowTicks() - last_render_time_) * playback_rate_;
    if (current_media_time > audio_clock_->back_timestamp())
      current_media_time = audio_clock_->back_timestamp();
  }

  return current_media_time;
}

bool AudioRendererImpl::GetWallClockTimes(
    const std::vector<base::TimeDelta>& media_timestamps,
    std::vector<base::TimeTicks>* wall_clock_times) {
  base::AutoLock auto_lock(lock_);
  DCHECK(wall_clock_times->empty());

  // When playback is paused (rate is zero), assume a rate of 1.0.
  const double playback_rate = playback_rate_ ? playback_rate_ : 1.0;
  const bool is_time_moving = sink_playing_ && playback_rate_ &&
                              !last_render_time_.is_null() &&
                              stop_rendering_time_.is_null() && !is_suspending_;

  // Pre-compute the time until playback of the audio buffer extents, since
  // these values are frequently used below.
  const base::TimeDelta time_until_front =
      audio_clock_->TimeUntilPlayback(audio_clock_->front_timestamp());
  const base::TimeDelta time_until_back =
      audio_clock_->TimeUntilPlayback(audio_clock_->back_timestamp());

  if (media_timestamps.empty()) {
    // Return the current media time as a wall clock time while accounting for
    // frames which may be in the process of play out.
    wall_clock_times->push_back(std::min(
        std::max(tick_clock_->NowTicks(), last_render_time_ + time_until_front),
        last_render_time_ + time_until_back));
    return is_time_moving;
  }

  wall_clock_times->reserve(media_timestamps.size());
  for (const auto& media_timestamp : media_timestamps) {
    // When time was or is moving and the requested media timestamp is within
    // range of played out audio, we can provide an exact conversion.
    if (!last_render_time_.is_null() &&
        media_timestamp >= audio_clock_->front_timestamp() &&
        media_timestamp <= audio_clock_->back_timestamp()) {
      wall_clock_times->push_back(
          last_render_time_ + audio_clock_->TimeUntilPlayback(media_timestamp));
      continue;
    }

    base::TimeDelta base_timestamp, time_until_playback;
    if (media_timestamp < audio_clock_->front_timestamp()) {
      base_timestamp = audio_clock_->front_timestamp();
      time_until_playback = time_until_front;
    } else {
      base_timestamp = audio_clock_->back_timestamp();
      time_until_playback = time_until_back;
    }

    // In practice, most calls will be estimates given the relatively small
    // window in which clients can get the actual time.
    wall_clock_times->push_back(last_render_time_ + time_until_playback +
                                (media_timestamp - base_timestamp) /
                                    playback_rate);
  }

  return is_time_moving;
}

TimeSource* AudioRendererImpl::GetTimeSource() {
  return this;
}

void AudioRendererImpl::Flush(const base::Closure& callback) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPlaying);
  DCHECK(flush_cb_.is_null());

  flush_cb_ = callback;
  ChangeState_Locked(kFlushing);

  if (pending_read_)
    return;

  ChangeState_Locked(kFlushed);
  DoFlush_Locked();
}

void AudioRendererImpl::DoFlush_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  DCHECK(!pending_read_);
  DCHECK_EQ(state_, kFlushed);

  ended_timestamp_ = kInfiniteDuration;
  audio_buffer_stream_->Reset(base::BindOnce(
      &AudioRendererImpl::ResetDecoderDone, weak_factory_.GetWeakPtr()));
}

void AudioRendererImpl::ResetDecoderDone() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock auto_lock(lock_);

    DCHECK_EQ(state_, kFlushed);
    DCHECK(!flush_cb_.is_null());

    received_end_of_stream_ = false;
    rendered_end_of_stream_ = false;

    // Flush() may have been called while underflowed/not fully buffered.
    if (buffering_state_ != BUFFERING_HAVE_NOTHING)
      SetBufferingState_Locked(BUFFERING_HAVE_NOTHING);

    if (buffer_converter_)
      buffer_converter_->Reset();
    algorithm_->FlushBuffers();
  }

  // Changes in buffering state are always posted. Flush callback must only be
  // run after buffering state has been set back to nothing.
  task_runner_->PostTask(FROM_HERE, base::ResetAndReturn(&flush_cb_));
}

void AudioRendererImpl::StartPlaying() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK(!sink_playing_);
  DCHECK_EQ(state_, kFlushed);
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);
  DCHECK(!pending_read_) << "Pending read must complete before seeking";

  ChangeState_Locked(kPlaying);
  AttemptRead_Locked();
}

void AudioRendererImpl::Initialize(DemuxerStream* stream,
                                   CdmContext* cdm_context,
                                   RendererClient* client,
                                   const PipelineStatusCB& init_cb) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(client);
  DCHECK(stream);
  DCHECK_EQ(stream->type(), DemuxerStream::AUDIO);
  DCHECK(!init_cb.is_null());
  DCHECK(state_ == kUninitialized || state_ == kFlushed);
  DCHECK(sink_.get());

  // Trying to track down AudioClock crash, http://crbug.com/674856.
  // Initialize should never be called while Rendering is ongoing. This can lead
  // to race conditions between media/audio-device threads.
  CHECK(!sink_playing_);
  // This lock is not required by Initialize, but failing to acquire the lock
  // would indicate a race with the rendering thread, which should not be active
  // at this time. This is just extra verification on the |sink_playing_| CHECK
  // above. We hold |lock_| while setting |sink_playing_|, but release the lock
  // when calling sink_->Pause() to avoid deadlocking with the AudioMixer.
  CHECK(lock_.Try());
  lock_.Release();

  // If we are re-initializing playback (e.g. switching media tracks), stop the
  // sink first.
  if (state_ == kFlushed) {
    sink_->Stop();
    audio_clock_.reset();
  }

  state_ = kInitializing;
  client_ = client;

  current_decoder_config_ = stream->audio_decoder_config();
  DCHECK(current_decoder_config_.IsValidConfig());

  auto output_device_info = sink_->GetOutputDeviceInfo();
  const AudioParameters& hw_params = output_device_info.output_params();
  ChannelLayout hw_channel_layout =
      hw_params.IsValid() ? hw_params.channel_layout() : CHANNEL_LAYOUT_NONE;

  audio_buffer_stream_ = std::make_unique<AudioBufferStream>(
      std::make_unique<AudioBufferStream::StreamTraits>(media_log_,
                                                        hw_channel_layout),
      task_runner_, create_audio_decoders_cb_, media_log_);

  audio_buffer_stream_->set_config_change_observer(base::Bind(
      &AudioRendererImpl::OnConfigChange, weak_factory_.GetWeakPtr()));

  // Always post |init_cb_| because |this| could be destroyed if initialization
  // failed.
  init_cb_ = BindToCurrentLoop(init_cb);

  AudioCodec codec = stream->audio_decoder_config().codec();
  if (auto* mc = GetMediaClient())
    is_passthrough_ = mc->IsSupportedBitstreamAudioCodec(codec);
  else
    is_passthrough_ = false;
  expecting_config_changes_ = stream->SupportsConfigChanges();

  bool use_stream_params = !expecting_config_changes_ || !hw_params.IsValid() ||
                           hw_params.format() == AudioParameters::AUDIO_FAKE ||
                           !sink_->IsOptimizedForHardwareParameters();

  if (stream->audio_decoder_config().channel_layout() ==
          CHANNEL_LAYOUT_DISCRETE &&
      sink_->IsOptimizedForHardwareParameters()) {
    use_stream_params = false;
  }

  // Target ~20ms for our buffer size (which is optimal for power efficiency and
  // responsiveness to play/pause events), but if the hardware needs something
  // even larger (say for Bluetooth devices) prefer that.
  //
  // Even if |use_stream_params| is true we should choose a value here based on
  // hardware parameters since it affects the initial buffer size used by
  // AudioRendererAlgorithm. Too small and we will underflow if the hardware
  // asks for a buffer larger than the initial algorithm capacity.
  const int preferred_buffer_size =
      std::max(2 * stream->audio_decoder_config().samples_per_second() / 100,
               hw_params.IsValid() ? hw_params.frames_per_buffer() : 0);

  if (is_passthrough_) {
    AudioParameters::Format format = AudioParameters::AUDIO_FAKE;
    if (codec == kCodecAC3) {
      format = AudioParameters::AUDIO_BITSTREAM_AC3;
    } else if (codec == kCodecEAC3) {
      format = AudioParameters::AUDIO_BITSTREAM_EAC3;
    } else {
      NOTREACHED();
    }

    // If we want the precise PCM frame count here, we have to somehow peek the
    // audio bitstream and parse the header ahead of time. Instead, we ensure
    // audio bus being large enough to accommodate
    // kMaxFramesPerCompressedAudioBuffer frames. The real data size and frame
    // count for bitstream formats will be carried in additional fields of
    // AudioBus.
    const int buffer_size =
        AudioParameters::kMaxFramesPerCompressedAudioBuffer *
        stream->audio_decoder_config().bytes_per_frame();

    audio_parameters_.Reset(
        format, stream->audio_decoder_config().channel_layout(),
        stream->audio_decoder_config().samples_per_second(), buffer_size);
    buffer_converter_.reset();
  } else if (use_stream_params) {
    audio_parameters_.Reset(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                            stream->audio_decoder_config().channel_layout(),
                            stream->audio_decoder_config().samples_per_second(),
                            preferred_buffer_size);
    audio_parameters_.set_channels_for_discrete(
        stream->audio_decoder_config().channels());
    buffer_converter_.reset();
  } else {
    // To allow for seamless sample rate adaptations (i.e. changes from say
    // 16kHz to 48kHz), always resample to the hardware rate.
    int sample_rate = hw_params.sample_rate();

    // If supported by the OS and the initial sample rate is not too low, let
    // the OS level resampler handle resampling for power efficiency.
    if (AudioLatency::IsResamplingPassthroughSupported(
            AudioLatency::LATENCY_PLAYBACK) &&
        stream->audio_decoder_config().samples_per_second() >= 44100) {
      sample_rate = stream->audio_decoder_config().samples_per_second();
    }

    int stream_channel_count = stream->audio_decoder_config().channels();

    bool try_supported_channel_layouts = false;
#if defined(OS_WIN)
    try_supported_channel_layouts =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kTrySupportedChannelLayouts);
#endif

    // We don't know how to up-mix for DISCRETE layouts (fancy multichannel
    // hardware with non-standard speaker arrangement). Instead, pretend the
    // hardware layout is stereo and let the OS take care of further up-mixing
    // to the discrete layout (http://crbug.com/266674). Additionally, pretend
    // hardware is stereo whenever kTrySupportedChannelLayouts is set. This flag
    // is for savvy users who want stereo content to output in all surround
    // speakers. Using the actual layout (likely 5.1 or higher) will mean our
    // mixer will attempt to up-mix stereo source streams to just the left/right
    // speaker of the 5.1 setup, nulling out the other channels
    // (http://crbug.com/177872).
    ChannelLayout hw_channel_layout =
        hw_params.channel_layout() == CHANNEL_LAYOUT_DISCRETE ||
                try_supported_channel_layouts
            ? CHANNEL_LAYOUT_STEREO
            : hw_params.channel_layout();
    int hw_channel_count = ChannelLayoutToChannelCount(hw_channel_layout);

    // The layout we pass to |audio_parameters_| will be used for the lifetime
    // of this audio renderer, regardless of changes to hardware and/or stream
    // properties. Below we choose the max of stream layout vs. hardware layout
    // to leave room for changes to the hardware and/or stream (i.e. avoid
    // premature down-mixing - http://crbug.com/379288).
    // If stream_channels < hw_channels:
    //   Taking max means we up-mix to hardware layout. If stream later changes
    //   to have more channels, we aren't locked into down-mixing to the
    //   initial stream layout.
    // If stream_channels > hw_channels:
    //   We choose to output stream's layout, meaning mixing is a no-op for the
    //   renderer. Browser-side will down-mix to the hardware config. If the
    //   hardware later changes to equal stream channels, browser-side will stop
    //   down-mixing and use the data from all stream channels.
    ChannelLayout renderer_channel_layout =
        hw_channel_count > stream_channel_count
            ? hw_channel_layout
            : stream->audio_decoder_config().channel_layout();

    audio_parameters_.Reset(hw_params.format(), renderer_channel_layout,
                            sample_rate,
                            media::AudioLatency::GetHighLatencyBufferSize(
                                sample_rate, preferred_buffer_size));
  }

  audio_parameters_.set_effects(audio_parameters_.effects() |
                                ::media::AudioParameters::MULTIZONE);

  audio_parameters_.set_latency_tag(AudioLatency::LATENCY_PLAYBACK);

  last_decoded_channel_layout_ =
      stream->audio_decoder_config().channel_layout();

  is_encrypted_ = stream->audio_decoder_config().is_encrypted();

  last_decoded_channels_ = stream->audio_decoder_config().channels();

  audio_clock_.reset(
      new AudioClock(base::TimeDelta(), audio_parameters_.sample_rate()));

  audio_buffer_stream_->Initialize(
      stream,
      base::BindOnce(&AudioRendererImpl::OnAudioBufferStreamInitialized,
                     weak_factory_.GetWeakPtr()),
      cdm_context,
      base::BindRepeating(&AudioRendererImpl::OnStatisticsUpdate,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&AudioRendererImpl::OnWaitingForDecryptionKey,
                          weak_factory_.GetWeakPtr()));
}

void AudioRendererImpl::OnAudioBufferStreamInitialized(bool success) {
  DVLOG(1) << __func__ << ": " << success;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);

  if (!success) {
    state_ = kUninitialized;
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  if (!audio_parameters_.IsValid()) {
    DVLOG(1) << __func__ << ": Invalid audio parameters: "
             << audio_parameters_.AsHumanReadableString();
    ChangeState_Locked(kUninitialized);
    // TODO(flim): If the channel layout is discrete but channel count is 0, a
    // possible cause is that the input stream has > 8 channels but there is no
    // Web Audio renderer attached and no channel mixing matrices defined for
    // hardware renderers. Adding one for previewing content could be useful.
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  if (expecting_config_changes_)
    buffer_converter_.reset(new AudioBufferConverter(audio_parameters_));

  // We're all good! Continue initializing the rest of the audio renderer
  // based on the decoder format.
  algorithm_.reset(new AudioRendererAlgorithm());
  algorithm_->Initialize(audio_parameters_, is_encrypted_);
  ConfigureChannelMask();

  ChangeState_Locked(kFlushed);

  {
    base::AutoUnlock auto_unlock(lock_);
    sink_->Initialize(audio_parameters_, this);
    sink_->Start();

    // Some sinks play on start...
    sink_->Pause();
  }

  DCHECK(!sink_playing_);
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

void AudioRendererImpl::OnPlaybackError(PipelineStatus error) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnError(error);
}

void AudioRendererImpl::OnPlaybackEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnEnded();
}

void AudioRendererImpl::OnStatisticsUpdate(const PipelineStatistics& stats) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnStatisticsUpdate(stats);
}

void AudioRendererImpl::OnBufferingStateChange(BufferingState state) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent(media_log_->CreateBufferingStateChangedEvent(
      "audio_buffering_state", state));
  client_->OnBufferingStateChange(state);
}

void AudioRendererImpl::OnWaitingForDecryptionKey() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnWaitingForDecryptionKey();
}

void AudioRendererImpl::SetVolume(float volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(sink_.get());
  sink_->SetVolume(volume);
}

void AudioRendererImpl::OnSuspend() {
  base::AutoLock auto_lock(lock_);
  is_suspending_ = true;
}

void AudioRendererImpl::OnResume() {
  base::AutoLock auto_lock(lock_);
  is_suspending_ = false;
}

void AudioRendererImpl::SetPlayDelayCBForTesting(PlayDelayCBForTesting cb) {
  DCHECK_EQ(state_, kUninitialized);
  play_delay_cb_for_testing_ = std::move(cb);
}

void AudioRendererImpl::DecodedAudioReady(
    AudioBufferStream::Status status,
    const scoped_refptr<AudioBuffer>& buffer) {
  DVLOG(2) << __func__ << "(" << status << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK(state_ != kUninitialized);

  CHECK(pending_read_);
  pending_read_ = false;

  if (status == AudioBufferStream::ABORTED ||
      status == AudioBufferStream::DEMUXER_READ_ABORTED) {
    HandleAbortedReadOrDecodeError(PIPELINE_OK);
    return;
  }

  if (status == AudioBufferStream::DECODE_ERROR) {
    HandleAbortedReadOrDecodeError(PIPELINE_ERROR_DECODE);
    return;
  }

  DCHECK_EQ(status, AudioBufferStream::OK);
  DCHECK(buffer.get());

  if (state_ == kFlushing) {
    ChangeState_Locked(kFlushed);
    DoFlush_Locked();
    return;
  }

  bool need_another_buffer = true;

  if (expecting_config_changes_) {
    if (!buffer->end_of_stream()) {
      if (last_decoded_sample_rate_ &&
          buffer->sample_rate() != last_decoded_sample_rate_) {
        DVLOG(1) << __func__ << " Updating audio sample_rate."
                 << " ts:" << buffer->timestamp().InMicroseconds()
                 << " old:" << last_decoded_sample_rate_
                 << " new:" << buffer->sample_rate();
        // Send a bogus config to reset timestamp state.
        OnConfigChange(AudioDecoderConfig());
      }
      last_decoded_sample_rate_ = buffer->sample_rate();

      if (last_decoded_channel_layout_ != buffer->channel_layout()) {
        if (buffer->channel_layout() == CHANNEL_LAYOUT_DISCRETE) {
          MEDIA_LOG(ERROR, media_log_)
              << "Unsupported midstream configuration change! Discrete channel"
              << " layout not allowed by sink.";
          HandleAbortedReadOrDecodeError(PIPELINE_ERROR_DECODE);
          return;
        } else {
          last_decoded_channel_layout_ = buffer->channel_layout();
          last_decoded_channels_ = buffer->channel_count();
          ConfigureChannelMask();
        }
      }
    }

    DCHECK(buffer_converter_);
    buffer_converter_->AddInput(buffer);

    while (buffer_converter_->HasNextBuffer()) {
      need_another_buffer =
          HandleDecodedBuffer_Locked(buffer_converter_->GetNextBuffer());
    }
  } else {
    // TODO(chcunningham, tguilbert): Figure out if we want to support implicit
    // config changes during src=. Doing so requires resampling each individual
    // stream which is inefficient when there are many tags in a page.
    //
    // Check if the buffer we received matches the expected configuration.
    // Note: We explicitly do not check channel layout here to avoid breaking
    // weird behavior with multichannel wav files: http://crbug.com/600538.
    if (!buffer->end_of_stream() &&
        (buffer->sample_rate() != audio_parameters_.sample_rate() ||
         buffer->channel_count() != audio_parameters_.channels())) {
      MEDIA_LOG(ERROR, media_log_)
          << "Unsupported midstream configuration change!"
          << " Sample Rate: " << buffer->sample_rate() << " vs "
          << audio_parameters_.sample_rate()
          << ", Channels: " << buffer->channel_count() << " vs "
          << audio_parameters_.channels();
      HandleAbortedReadOrDecodeError(PIPELINE_ERROR_DECODE);
      return;
    }

    need_another_buffer = HandleDecodedBuffer_Locked(buffer);
  }

  if (!need_another_buffer && !CanRead_Locked())
    return;

  AttemptRead_Locked();
}

bool AudioRendererImpl::HandleDecodedBuffer_Locked(
    const scoped_refptr<AudioBuffer>& buffer) {
  lock_.AssertAcquired();
  if (buffer->end_of_stream()) {
    received_end_of_stream_ = true;
  } else {
    if (buffer->IsBitstreamFormat() && state_ == kPlaying) {
      if (IsBeforeStartTime(buffer))
        return true;

      // Adjust the start time since we are unable to trim a compressed audio
      // buffer.
      if (buffer->timestamp() < start_timestamp_ &&
          (buffer->timestamp() + buffer->duration()) > start_timestamp_) {
        start_timestamp_ = buffer->timestamp();
        audio_clock_.reset(new AudioClock(buffer->timestamp(),
                                          audio_parameters_.sample_rate()));
      }
    } else if (state_ == kPlaying) {
      if (IsBeforeStartTime(buffer))
        return true;

      // Trim off any additional time before the start timestamp.
      const base::TimeDelta trim_time = start_timestamp_ - buffer->timestamp();
      if (trim_time > base::TimeDelta()) {
        buffer->TrimStart(buffer->frame_count() *
                          (static_cast<double>(trim_time.InMicroseconds()) /
                           buffer->duration().InMicroseconds()));
        buffer->set_timestamp(start_timestamp_);
      }
      // If the entire buffer was trimmed, request a new one.
      if (!buffer->frame_count())
        return true;
    }

    if (state_ != kUninitialized)
      algorithm_->EnqueueBuffer(buffer);
  }

  // Store the timestamp of the first packet so we know when to start actual
  // audio playback.
  if (first_packet_timestamp_ == kNoTimestamp)
    first_packet_timestamp_ = buffer->timestamp();

  const size_t memory_usage = algorithm_->GetMemoryUsage();
  PipelineStatistics stats;
  stats.audio_memory_usage = memory_usage - last_audio_memory_usage_;
  last_audio_memory_usage_ = memory_usage;
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&AudioRendererImpl::OnStatisticsUpdate,
                                    weak_factory_.GetWeakPtr(), stats));

  switch (state_) {
    case kUninitialized:
    case kInitializing:
    case kFlushing:
      NOTREACHED();
      return false;

    case kFlushed:
      DCHECK(!pending_read_);
      return false;

    case kPlaying:
      if (buffer->end_of_stream() || algorithm_->IsQueueFull()) {
        if (buffering_state_ == BUFFERING_HAVE_NOTHING)
          SetBufferingState_Locked(BUFFERING_HAVE_ENOUGH);
        return false;
      }
      return true;
  }
  return false;
}

void AudioRendererImpl::AttemptRead() {
  base::AutoLock auto_lock(lock_);
  AttemptRead_Locked();
}

void AudioRendererImpl::AttemptRead_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (!CanRead_Locked())
    return;

  pending_read_ = true;
  audio_buffer_stream_->Read(base::BindOnce(
      &AudioRendererImpl::DecodedAudioReady, weak_factory_.GetWeakPtr()));
}

bool AudioRendererImpl::CanRead_Locked() {
  lock_.AssertAcquired();

  switch (state_) {
    case kUninitialized:
    case kInitializing:
    case kFlushing:
    case kFlushed:
      return false;

    case kPlaying:
      break;
  }

  return !pending_read_ && !received_end_of_stream_ &&
      !algorithm_->IsQueueFull();
}

void AudioRendererImpl::SetPlaybackRate(double playback_rate) {
  DVLOG(1) << __func__ << "(" << playback_rate << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_GE(playback_rate, 0);
  DCHECK(sink_.get());

  base::AutoLock auto_lock(lock_);

  if (is_passthrough_ && playback_rate != 0 && playback_rate != 1) {
    MEDIA_LOG(INFO, media_log_) << "Playback rate changes are not supported "
                                   "when output compressed bitstream."
                                << " Playback Rate: " << playback_rate;
    return;
  }

  // We have two cases here:
  // Play: current_playback_rate == 0 && playback_rate != 0
  // Pause: current_playback_rate != 0 && playback_rate == 0
  double current_playback_rate = playback_rate_;
  playback_rate_ = playback_rate;

  if (!rendering_)
    return;

  if (current_playback_rate == 0 && playback_rate != 0) {
    StartRendering_Locked();
    return;
  }

  if (current_playback_rate != 0 && playback_rate == 0) {
    StopRendering_Locked();
    return;
  }
}

bool AudioRendererImpl::IsBeforeStartTime(
    const scoped_refptr<AudioBuffer>& buffer) {
  DCHECK_EQ(state_, kPlaying);
  return buffer.get() && !buffer->end_of_stream() &&
         (buffer->timestamp() + buffer->duration()) < start_timestamp_;
}

int AudioRendererImpl::Render(base::TimeDelta delay,
                              base::TimeTicks delay_timestamp,
                              int prior_frames_skipped,
                              AudioBus* audio_bus) {
  TRACE_EVENT1("media", "AudioRendererImpl::Render", "id", media_log_->id());
  int frames_requested = audio_bus->frames();
  DVLOG(4) << __func__ << " delay:" << delay
           << " prior_frames_skipped:" << prior_frames_skipped
           << " frames_requested:" << frames_requested;

  int frames_written = 0;
  {
    base::AutoLock auto_lock(lock_);
    last_render_time_ = tick_clock_->NowTicks();

    int64_t frames_delayed = AudioTimestampHelper::TimeToFrames(
        delay, audio_parameters_.sample_rate());

    if (!stop_rendering_time_.is_null()) {
      audio_clock_->CompensateForSuspendedWrites(
          last_render_time_ - stop_rendering_time_, frames_delayed);
      stop_rendering_time_ = base::TimeTicks();
    }

    // Ensure Stop() hasn't destroyed our |algorithm_| on the pipeline thread.
    if (!algorithm_) {
      audio_clock_->WroteAudio(0, frames_requested, frames_delayed,
                               playback_rate_);
      return 0;
    }

    if (playback_rate_ == 0 || is_suspending_) {
      audio_clock_->WroteAudio(0, frames_requested, frames_delayed,
                               playback_rate_);
      return 0;
    }

    // Mute audio by returning 0 when not playing.
    if (state_ != kPlaying) {
      audio_clock_->WroteAudio(0, frames_requested, frames_delayed,
                               playback_rate_);
      return 0;
    }

    if (is_passthrough_ && algorithm_->frames_buffered() > 0) {
      // TODO(tsunghung): For compressed bitstream formats, play zeroed buffer
      // won't generate delay. It could be discarded immediately. Need another
      // way to generate audio delay.
      const base::TimeDelta play_delay =
          first_packet_timestamp_ - audio_clock_->back_timestamp();
      if (play_delay > base::TimeDelta()) {
        MEDIA_LOG(ERROR, media_log_)
            << "Cannot add delay for compressed audio bitstream foramt."
            << " Requested delay: " << play_delay;
      }

      frames_written += algorithm_->FillBuffer(audio_bus, 0, frames_requested,
                                               playback_rate_);

      // See Initialize(), the |audio_bus| should be bigger than we need in
      // bitstream cases. Fix |frames_requested| to avoid incorrent time
      // calculation of |audio_clock_| below.
      frames_requested = frames_written;
    } else if (algorithm_->frames_buffered() > 0) {
      // Delay playback by writing silence if we haven't reached the first
      // timestamp yet; this can occur if the video starts before the audio.
      CHECK_NE(first_packet_timestamp_, kNoTimestamp);
      CHECK_GE(first_packet_timestamp_, base::TimeDelta());
      const base::TimeDelta play_delay =
          first_packet_timestamp_ - audio_clock_->back_timestamp();
      if (play_delay > base::TimeDelta()) {
        DCHECK_EQ(frames_written, 0);

        if (!play_delay_cb_for_testing_.is_null())
          play_delay_cb_for_testing_.Run(play_delay);

        // Don't multiply |play_delay| out since it can be a huge value on
        // poorly encoded media and multiplying by the sample rate could cause
        // the value to overflow.
        if (play_delay.InSecondsF() > static_cast<double>(frames_requested) /
                                          audio_parameters_.sample_rate()) {
          frames_written = frames_requested;
        } else {
          frames_written =
              play_delay.InSecondsF() * audio_parameters_.sample_rate();
        }

        audio_bus->ZeroFramesPartial(0, frames_written);
      }

      // If there's any space left, actually render the audio; this is where the
      // aural magic happens.
      if (frames_written < frames_requested) {
        frames_written += algorithm_->FillBuffer(
            audio_bus, frames_written, frames_requested - frames_written,
            playback_rate_);
      }
    }

    // We use the following conditions to determine end of playback:
    //   1) Algorithm can not fill the audio callback buffer
    //   2) We received an end of stream buffer
    //   3) We haven't already signalled that we've ended
    //   4) We've played all known audio data sent to hardware
    //
    // We use the following conditions to determine underflow:
    //   1) Algorithm can not fill the audio callback buffer
    //   2) We have NOT received an end of stream buffer
    //   3) We are in the kPlaying state
    //
    // Otherwise the buffer has data we can send to the device.
    //
    // Per the TimeSource API the media time should always increase even after
    // we've rendered all known audio data. Doing so simplifies scenarios where
    // we have other sources of media data that need to be scheduled after audio
    // data has ended.
    //
    // That being said, we don't want to advance time when underflowed as we
    // know more decoded frames will eventually arrive. If we did, we would
    // throw things out of sync when said decoded frames arrive.
    int frames_after_end_of_stream = 0;
    if (frames_written == 0) {
      if (received_end_of_stream_) {
        if (ended_timestamp_ == kInfiniteDuration)
          ended_timestamp_ = audio_clock_->back_timestamp();
        frames_after_end_of_stream = frames_requested;
      } else if (state_ == kPlaying &&
                 buffering_state_ != BUFFERING_HAVE_NOTHING) {
        algorithm_->IncreaseQueueCapacity();
        SetBufferingState_Locked(BUFFERING_HAVE_NOTHING);
      }
    } else if (frames_written < frames_requested && !received_end_of_stream_) {
      // If we only partially filled the request and should have more data, go
      // ahead and increase queue capacity to try and meet the next request.
      algorithm_->IncreaseQueueCapacity();
    }

    audio_clock_->WroteAudio(frames_written + frames_after_end_of_stream,
                             frames_requested, frames_delayed, playback_rate_);

    if (CanRead_Locked()) {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&AudioRendererImpl::AttemptRead,
                                        weak_factory_.GetWeakPtr()));
    }

    if (audio_clock_->front_timestamp() >= ended_timestamp_ &&
        !rendered_end_of_stream_) {
      rendered_end_of_stream_ = true;
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&AudioRendererImpl::OnPlaybackEnded,
                                        weak_factory_.GetWeakPtr()));
    }
  }

  DCHECK_LE(frames_written, frames_requested);
  return frames_written;
}

void AudioRendererImpl::OnRenderError() {
  MEDIA_LOG(ERROR, media_log_) << "audio render error";

  // Post to |task_runner_| as this is called on the audio callback thread.
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&AudioRendererImpl::OnPlaybackError,
                            weak_factory_.GetWeakPtr(), AUDIO_RENDERER_ERROR));
}

void AudioRendererImpl::HandleAbortedReadOrDecodeError(PipelineStatus status) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  switch (state_) {
    case kUninitialized:
    case kInitializing:
      NOTREACHED();
      return;
    case kFlushing:
      ChangeState_Locked(kFlushed);
      if (status == PIPELINE_OK) {
        DoFlush_Locked();
        return;
      }

      MEDIA_LOG(ERROR, media_log_) << "audio error during flushing, status: "
                                   << MediaLog::PipelineStatusToString(status);
      client_->OnError(status);
      base::ResetAndReturn(&flush_cb_).Run();
      return;

    case kFlushed:
    case kPlaying:
      if (status != PIPELINE_OK) {
        MEDIA_LOG(ERROR, media_log_)
            << "audio error during playing, status: "
            << MediaLog::PipelineStatusToString(status);
        client_->OnError(status);
      }
      return;
  }
}

void AudioRendererImpl::ChangeState_Locked(State new_state) {
  DVLOG(1) << __func__ << " : " << state_ << " -> " << new_state;
  lock_.AssertAcquired();
  state_ = new_state;
}

void AudioRendererImpl::OnConfigChange(const AudioDecoderConfig& config) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(expecting_config_changes_);
  buffer_converter_->ResetTimestampState();

  // An invalid config may be supplied by callers who simply want to reset
  // internal state outside of detecting a new config from the demuxer stream.
  // RendererClient only cares to know about config changes that differ from
  // previous configs.
  if (config.IsValidConfig() && !current_decoder_config_.Matches(config)) {
    current_decoder_config_ = config;
    client_->OnAudioConfigChange(config);
  }
}

void AudioRendererImpl::SetBufferingState_Locked(
    BufferingState buffering_state) {
  DVLOG(1) << __func__ << " : " << buffering_state_ << " -> "
           << buffering_state;
  DCHECK_NE(buffering_state_, buffering_state);
  lock_.AssertAcquired();
  buffering_state_ = buffering_state;

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&AudioRendererImpl::OnBufferingStateChange,
                            weak_factory_.GetWeakPtr(), buffering_state_));
}

void AudioRendererImpl::ConfigureChannelMask() {
  DCHECK(algorithm_);
  DCHECK(audio_parameters_.IsValid());
  DCHECK_NE(last_decoded_channel_layout_, CHANNEL_LAYOUT_NONE);
  DCHECK_NE(last_decoded_channel_layout_, CHANNEL_LAYOUT_UNSUPPORTED);

  // If we're actually downmixing the signal, no mask is necessary, but ensure
  // we clear any existing mask if present.
  if (last_decoded_channels_ >= audio_parameters_.channels()) {
    algorithm_->SetChannelMask(
        std::vector<bool>(audio_parameters_.channels(), true));
    return;
  }

  // Determine the matrix used to upmix the channels.
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix(last_decoded_channel_layout_, last_decoded_channels_,
                      audio_parameters_.channel_layout(),
                      audio_parameters_.channels())
      .CreateTransformationMatrix(&matrix);

  // All channels with a zero mix are muted and can be ignored.
  std::vector<bool> channel_mask(audio_parameters_.channels(), false);
  for (size_t ch = 0; ch < matrix.size(); ++ch) {
    channel_mask[ch] = std::any_of(matrix[ch].begin(), matrix[ch].end(),
                                   [](float mix) { return !!mix; });
  }
  algorithm_->SetChannelMask(std::move(channel_mask));
}

}  // namespace media
