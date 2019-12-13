// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/audio/fuchsia_audio_renderer.h"

#include <lib/sys/cpp/component_context.h>

#include "base/bind.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/base/decoder_buffer.h"
#include "media/base/renderer_client.h"
#include "media/filters/decrypting_demuxer_stream.h"

namespace media {

namespace {

// nullopt is returned in case the codec is not supported. nullptr is returned
// for uncompressed PCM streams.
base::Optional<std::unique_ptr<fuchsia::media::Compression>>
GetFuchsiaCompressionFromAudioCodec(AudioCodec codec) {
  auto compression = std::make_unique<fuchsia::media::Compression>();
  switch (codec) {
    case kCodecAAC:
      compression->type = fuchsia::media::AUDIO_ENCODING_AAC;
      break;
    case kCodecMP3:
      compression->type = fuchsia::media::AUDIO_ENCODING_MP3;
      break;
    case kCodecVorbis:
      compression->type = fuchsia::media::AUDIO_ENCODING_VORBIS;
      break;
    case kCodecOpus:
      compression->type = fuchsia::media::AUDIO_ENCODING_OPUS;
      break;
    case kCodecFLAC:
      compression->type = fuchsia::media::AUDIO_ENCODING_FLAC;
      break;
    case kCodecPCM:
      compression.reset();
      break;

    default:
      return base::nullopt;
  }
  return std::move(compression);
}

base::Optional<fuchsia::media::AudioSampleFormat>
GetFuchsiaSampleFormatFromSampleFormat(SampleFormat sample_format) {
  switch (sample_format) {
    case kSampleFormatU8:
      return fuchsia::media::AudioSampleFormat::UNSIGNED_8;
    case kSampleFormatS16:
      return fuchsia::media::AudioSampleFormat::SIGNED_16;
    case kSampleFormatS24:
      return fuchsia::media::AudioSampleFormat::SIGNED_24_IN_32;
    case kSampleFormatF32:
      return fuchsia::media::AudioSampleFormat::FLOAT;

    default:
      return base::nullopt;
  }
}

}  // namespace

// Size of a single audio buffer: 100kB. It's enough to cover 100ms of PCM at
// 48kHz, 2 channels, 16 bps.
constexpr size_t kBufferSize = 100 * 1024;

// Total number of buffers. 16 is the maximum allowed by AudioConsumer.
constexpr size_t kNumBuffers = 16;

FuchsiaAudioRenderer::FuchsiaAudioRenderer(
    MediaLog* media_log,
    mojo::PendingRemote<media::mojom::FuchsiaAudioConsumerProvider>
        pending_audio_consumer_provider)
    : media_log_(media_log) {
  DETACH_FROM_THREAD(thread_checker_);

  mojo::Remote<media::mojom::FuchsiaAudioConsumerProvider>
      audio_consumer_provider;
  audio_consumer_provider.Bind(std::move(pending_audio_consumer_provider));
  audio_consumer_provider->CreateAudioConsumer(
      audio_consumer_handle_.NewRequest());
}

FuchsiaAudioRenderer::~FuchsiaAudioRenderer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void FuchsiaAudioRenderer::Initialize(DemuxerStream* stream,
                                      CdmContext* cdm_context,
                                      RendererClient* client,
                                      const PipelineStatusCB& init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!demuxer_stream_);

  DCHECK(!init_cb_);
  init_cb_ = std::move(init_cb);

  demuxer_stream_ = stream;
  client_ = client;

  audio_consumer_.Bind(std::move(audio_consumer_handle_));
  audio_consumer_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "AudioConsumer disconnected.";
    OnError(AUDIO_RENDERER_ERROR);
  });

  audio_consumer_.events().OnEndOfStream = [this]() { OnEndOfStream(); };
  RequestAudioConsumerStatus();

  // Allocate input buffers for the StreamSink.
  stream_sink_buffers_.resize(kNumBuffers);
  std::vector<zx::vmo> vmos_for_stream_sink;
  vmos_for_stream_sink.reserve(kNumBuffers);
  for (StreamSinkBuffer& buffer : stream_sink_buffers_) {
    zx_status_t status = zx::vmo::create(kBufferSize, 0, &buffer.vmo);
    ZX_CHECK(status == ZX_OK, status) << "zx_vmo_create";

    constexpr char kName[] = "cr-audio-renderer";
    status =
        buffer.vmo.set_property(ZX_PROP_NAME, kName, base::size(kName) - 1);
    ZX_DCHECK(status == ZX_OK, status);

    // Duplicate VMO handle to pass to AudioConsumer.
    zx::vmo readonly_vmo;
    status = buffer.vmo.duplicate(ZX_RIGHT_DUPLICATE | ZX_RIGHT_TRANSFER |
                                      ZX_RIGHT_READ | ZX_RIGHT_MAP |
                                      ZX_RIGHT_GET_PROPERTY,
                                  &readonly_vmo);
    ZX_CHECK(status == ZX_OK, status) << "zx_handle_duplicate";

    vmos_for_stream_sink.push_back(std::move(readonly_vmo));
  }

  AudioDecoderConfig config = demuxer_stream_->audio_decoder_config();

  auto compression = GetFuchsiaCompressionFromAudioCodec(config.codec());
  if (!compression) {
    LOG(ERROR) << "Unsupported audio codec: " << GetCodecName(config.codec());
    std::move(init_cb_).Run(AUDIO_RENDERER_ERROR);
    return;
  }

  fuchsia::media::AudioStreamType stream_type;
  stream_type.channels = config.channels();
  stream_type.frames_per_second = config.samples_per_second();

  // Set sample_format for uncompressed streams.
  if (!compression) {
    base::Optional<fuchsia::media::AudioSampleFormat> sample_format =
        GetFuchsiaSampleFormatFromSampleFormat(config.sample_format());
    if (!sample_format) {
      LOG(ERROR) << "Unsupported sample format: "
                 << SampleFormatToString(config.sample_format());
      std::move(init_cb_).Run(AUDIO_RENDERER_ERROR);
      return;
    }
    stream_type.sample_format = sample_format.value();
  } else {
    // For compressed formats sample format is determined by the decoder, but
    // this field is still required in AudioStreamType.
    stream_type.sample_format = fuchsia::media::AudioSampleFormat::SIGNED_16;
  }

  audio_consumer_->CreateStreamSink(
      std::move(vmos_for_stream_sink), std::move(stream_type),
      std::move(compression).value(), stream_sink_.NewRequest());

  // If the stream is encrypted then initialize |decrypting_demuxer_stream_|.
  if (demuxer_stream_->audio_decoder_config().is_encrypted() && cdm_context) {
    WaitingCB waiting_cb = base::BindRepeating(&RendererClient::OnWaiting,
                                               base::Unretained(client_));
    decrypting_demuxer_stream_ = std::make_unique<DecryptingDemuxerStream>(
        base::ThreadTaskRunnerHandle::Get(), media_log_, waiting_cb);
    decrypting_demuxer_stream_->Initialize(
        demuxer_stream_, cdm_context,
        base::BindRepeating(&FuchsiaAudioRenderer::OnDecryptorInitialized,
                            base::Unretained(this)));
    return;
  }

  std::move(init_cb_).Run(PIPELINE_OK);
}

TimeSource* FuchsiaAudioRenderer::GetTimeSource() {
  return this;
}

void FuchsiaAudioRenderer::Flush(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  FlushInternal();
  std::move(callback).Run();
}

void FuchsiaAudioRenderer::StartPlaying() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ScheduleReadDemuxerStream();
}

void FuchsiaAudioRenderer::SetVolume(float volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!volume_control_) {
    audio_consumer_->BindVolumeControl(volume_control_.NewRequest());
    volume_control_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "VolumeControl disconnected.";
    });
  }
  volume_control_->SetVolume(volume);
}

void FuchsiaAudioRenderer::StartTicking() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  fuchsia::media::AudioConsumerStartFlags flags{};
  if (demuxer_stream_->liveness() == DemuxerStream::LIVENESS_LIVE) {
    flags = fuchsia::media::AudioConsumerStartFlags::LOW_LATENCY;
  }

  base::TimeDelta media_pos;
  {
    base::AutoLock lock(state_lock_);
    media_pos = media_pos_;
    state_ = PlaybackState::kStarting;
  }

  audio_consumer_->Start(flags, fuchsia::media::NO_TIMESTAMP,
                         media_pos.ToZxDuration());
}

void FuchsiaAudioRenderer::StopTicking() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  audio_consumer_->Stop();

  base::AutoLock lock(state_lock_);
  DCHECK(state_ != PlaybackState::kStopped);
  state_ = PlaybackState::kStopped;
  media_pos_ = CurrentMediaTimeLocked();
}

void FuchsiaAudioRenderer::SetPlaybackRate(double playback_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  audio_consumer_->SetRate(playback_rate);
}

void FuchsiaAudioRenderer::SetMediaTime(base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  {
    base::AutoLock lock(state_lock_);
    DCHECK(state_ == PlaybackState::kStopped);
    media_pos_ = time;
  }

  FlushInternal();
  ScheduleReadDemuxerStream();
}

base::TimeDelta FuchsiaAudioRenderer::CurrentMediaTime() {
  base::AutoLock lock(state_lock_);
  if (state_ != PlaybackState::kPlaying)
    return media_pos_;

  return CurrentMediaTimeLocked();
}

bool FuchsiaAudioRenderer::GetWallClockTimes(
    const std::vector<base::TimeDelta>& media_timestamps,
    std::vector<base::TimeTicks>* wall_clock_times) {
  wall_clock_times->reserve(media_timestamps.size());
  auto now = base::TimeTicks::Now();

  base::AutoLock lock(state_lock_);

  const bool is_time_moving = state_ == PlaybackState::kPlaying;

  if (media_timestamps.empty()) {
    wall_clock_times->push_back(is_time_moving ? now : base::TimeTicks());
    return is_time_moving;
  }

  base::TimeTicks wall_clock_base = is_time_moving ? reference_time_ : now;

  for (base::TimeDelta timestamp : media_timestamps) {
    base::TimeTicks wall_clock_time;

    auto relative_pos = timestamp - media_pos_;
    if (is_time_moving) {
      // See https://fuchsia.dev/reference/fidl/fuchsia.media#formulas .
      relative_pos = relative_pos * reference_delta_ / media_delta_;
    }
    wall_clock_time = wall_clock_base + relative_pos;
    wall_clock_times->push_back(wall_clock_time);
  }

  return is_time_moving;
}

void FuchsiaAudioRenderer::OnError(PipelineStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  audio_consumer_.Unbind();
  stream_sink_.Unbind();
  if (init_cb_) {
    std::move(init_cb_).Run(status);
  } else if (client_) {
    client_->OnError(status);
  }
}

void FuchsiaAudioRenderer::OnDecryptorInitialized(PipelineStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!init_cb_)
    return;

  if (status == PIPELINE_OK) {
    demuxer_stream_ = decrypting_demuxer_stream_.get();
  }

  std::move(init_cb_).Run(status);
}

void FuchsiaAudioRenderer::RequestAudioConsumerStatus() {
  audio_consumer_->WatchStatus(fit::bind_member(
      this, &FuchsiaAudioRenderer::OnAudioConsumerStatusChanged));
}

void FuchsiaAudioRenderer::OnAudioConsumerStatusChanged(
    fuchsia::media::AudioConsumerStatus status) {
  if (status.has_error()) {
    LOG(ERROR) << "fuchsia::media::AudioConsumer reported an error";
    OnError(AUDIO_RENDERER_ERROR);
    return;
  }

  if (status.has_presentation_timeline()) {
    bool playing = status.presentation_timeline().subject_delta > 0;

    base::AutoLock lock(state_lock_);
    if (playing && state_ != PlaybackState::kStopped) {
      if (state_ == PlaybackState::kStarting) {
        state_ = PlaybackState::kPlaying;
      }
      reference_time_ = base::TimeTicks::FromZxTime(
          status.presentation_timeline().reference_time);
      media_pos_ = base::TimeDelta::FromZxDuration(
          status.presentation_timeline().subject_delta);
      reference_delta_ = status.presentation_timeline().reference_delta;
      media_delta_ = status.presentation_timeline().subject_delta;
    }
  }

  bool reschedule_read_timer = false;
  if (status.has_min_lead_time()) {
    auto new_min_lead_time =
        base::TimeDelta::FromZxDuration(status.min_lead_time());
    DCHECK(!new_min_lead_time.is_zero());
    if (new_min_lead_time != min_lead_time_) {
      min_lead_time_ = new_min_lead_time;
      reschedule_read_timer = true;
    }
  }
  if (status.has_max_lead_time()) {
    auto new_max_lead_time =
        base::TimeDelta::FromZxDuration(status.max_lead_time());
    DCHECK(!new_max_lead_time.is_zero());
    if (new_max_lead_time != max_lead_time_) {
      max_lead_time_ = new_max_lead_time;
      reschedule_read_timer = true;
    }
  }

  if (reschedule_read_timer) {
    read_timer_.Stop();
    ScheduleReadDemuxerStream();
  }

  RequestAudioConsumerStatus();
}

void FuchsiaAudioRenderer::ScheduleReadDemuxerStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bool at_end_of_stream;
  {
    base::AutoLock lock(state_lock_);
    at_end_of_stream = state_ == PlaybackState::kEndOfStream;
    // |state_| cannot be changed on any other thread, so |at_end_of_stream|
    // will be correct after the lock is released. The lock is required only to
    // appease static thread annotations checker.
  }

  if (read_timer_.IsRunning() || demuxer_stream_->IsReadPending() ||
      at_end_of_stream || num_pending_packets_ >= stream_sink_buffers_.size()) {
    return;
  }

  base::TimeDelta next_read_delay;
  if (!last_packet_timestamp_.is_min()) {
    auto relative_buffer_pos = last_packet_timestamp_ - CurrentMediaTime();

    if (!min_lead_time_.is_zero() && relative_buffer_pos > min_lead_time_) {
      SetBufferState(BUFFERING_HAVE_ENOUGH);
    }

    if (!max_lead_time_.is_zero() && relative_buffer_pos > max_lead_time_) {
      next_read_delay = relative_buffer_pos - max_lead_time_;
    }
  }

  read_timer_.Start(FROM_HERE, next_read_delay,
                    base::BindOnce(&FuchsiaAudioRenderer::ReadDemuxerStream,
                                   base::Unretained(this)));
}

void FuchsiaAudioRenderer::ReadDemuxerStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  demuxer_stream_->Read(
      base::BindOnce(&FuchsiaAudioRenderer::OnDemuxerStreamReadDone,
                     weak_factory_.GetWeakPtr()));
}

void FuchsiaAudioRenderer::OnDemuxerStreamReadDone(
    DemuxerStream::Status read_status,
    scoped_refptr<DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (read_status != DemuxerStream::kOk) {
    if (read_status == DemuxerStream::kError) {
      OnError(PIPELINE_ERROR_READ);
    } else if (read_status == DemuxerStream::kConfigChanged) {
      LOG(ERROR)
          << "FuchsiaAudioRenderer doesn't support mid-stream config change.";
      OnError(PIPELINE_ERROR_READ);
    } else {
      DCHECK_EQ(read_status, DemuxerStream::kAborted);
    }
    return;
  }

  if (buffer->end_of_stream()) {
    base::AutoLock lock(state_lock_);
    state_ = PlaybackState::kEndOfStream;
    stream_sink_->EndOfStream();
    return;
  }

  if (buffer->data_size() > kBufferSize) {
    DLOG(ERROR) << "Demuxer returned buffer that is too big: "
                << buffer->data_size();
    OnError(AUDIO_RENDERER_ERROR);
    return;
  }

  // Find unused buffer.
  auto it = std::find_if(
      stream_sink_buffers_.begin(), stream_sink_buffers_.end(),
      [](const StreamSinkBuffer& b) -> bool { return !b.is_used; });

  // ReadDemuxerStream() is not supposed to be called unless there are unused
  // buffers.
  CHECK(it != stream_sink_buffers_.end());

  ++num_pending_packets_;
  DCHECK_LE(num_pending_packets_, stream_sink_buffers_.size());

  it->is_used = true;
  zx_status_t status = it->vmo.write(buffer->data(), 0, buffer->data_size());
  ZX_CHECK(status == ZX_OK, status) << "zx_vmo_write";

  size_t buffer_index = it - stream_sink_buffers_.begin();

  fuchsia::media::StreamPacket packet;
  packet.payload_buffer_id = buffer_index;
  packet.pts = buffer->timestamp().ToZxDuration();
  packet.payload_offset = 0;
  packet.payload_size = buffer->data_size();

  stream_sink_->SendPacket(std::move(packet), [this, buffer_index]() {
    OnStreamSendDone(buffer_index);
  });

  last_packet_timestamp_ = buffer->timestamp();

  ScheduleReadDemuxerStream();
}

void FuchsiaAudioRenderer::OnStreamSendDone(size_t buffer_index) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_LT(buffer_index, stream_sink_buffers_.size());
  DCHECK(stream_sink_buffers_[buffer_index].is_used);
  stream_sink_buffers_[buffer_index].is_used = false;

  DCHECK_GT(num_pending_packets_, 0U);
  --num_pending_packets_;

  ScheduleReadDemuxerStream();
}

void FuchsiaAudioRenderer::SetBufferState(BufferingState buffer_state) {
  if (buffer_state != buffer_state_) {
    buffer_state_ = buffer_state;
    client_->OnBufferingStateChange(buffer_state_,
                                    BUFFERING_CHANGE_REASON_UNKNOWN);
  }
}

void FuchsiaAudioRenderer::FlushInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  {
    base::AutoLock lock(state_lock_);
    DCHECK(state_ == PlaybackState::kStopped);
  }

  stream_sink_->DiscardAllPacketsNoReply();
  SetBufferState(BUFFERING_HAVE_NOTHING);
  last_packet_timestamp_ = base::TimeDelta::Min();
  read_timer_.Stop();
}

void FuchsiaAudioRenderer::OnEndOfStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->OnEnded();
}

base::TimeDelta FuchsiaAudioRenderer::CurrentMediaTimeLocked() {
  // Calculate media position using formula specified by the TimelineFunction.
  // See https://fuchsia.dev/reference/fidl/fuchsia.media#formulas .
  return media_pos_ + (base::TimeTicks::Now() - reference_time_) *
                          media_delta_ / reference_delta_;
}

}  // namespace media