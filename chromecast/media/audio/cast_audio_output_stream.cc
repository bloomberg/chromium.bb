// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_output_stream.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/base/media_message_loop.h"
#include "chromecast/media/cma/base/cast_decoder_buffer_impl.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/decrypt_context.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"

namespace chromecast {
namespace media {
namespace {

MediaPipelineBackend::AudioDecoder* InitializeBackend(
    const ::media::AudioParameters& audio_params,
    MediaPipelineBackend* backend,
    MediaPipelineBackend::Delegate* delegate) {
  DCHECK(backend);
  DCHECK(delegate);

  MediaPipelineBackend::AudioDecoder* decoder = backend->CreateAudioDecoder();
  if (!decoder)
    return nullptr;

  AudioConfig audio_config;
  audio_config.codec = kCodecPCM;
  audio_config.sample_format = kSampleFormatS16;
  audio_config.bytes_per_channel = audio_params.bits_per_sample() / 8;
  audio_config.channel_number = audio_params.channels();
  audio_config.samples_per_second = audio_params.sample_rate();
  audio_config.extra_data = nullptr;
  audio_config.extra_data_size = 0;
  audio_config.is_encrypted = false;

  if (!decoder->SetConfig(audio_config))
    return nullptr;

  if (!backend->Initialize(delegate))
    return nullptr;

  return decoder;
}

}  // namespace

// Backend represents a MediaPipelineBackend adapter that runs on cast
// media thread (media::MediaMessageLoop::GetTaskRunner).
// It can be created and destroyed on any thread, but all other member functions
// must be called on a single thread.
class CastAudioOutputStream::Backend : public MediaPipelineBackend::Delegate {
 public:
  typedef base::Callback<void(bool)> PushBufferCompletionCallback;

  Backend(const ::media::AudioParameters& audio_params)
      : audio_params_(audio_params),
        decoder_(nullptr),
        first_start_(true),
        error_(false),
        backend_buffer_(nullptr) {
    thread_checker_.DetachFromThread();
  }
  ~Backend() override {}

  void Open(CastAudioManager* audio_manager,
            bool* success,
            base::WaitableEvent* completion_event) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(backend_ == nullptr);
    DCHECK(audio_manager);
    DCHECK(success);
    DCHECK(completion_event);

    backend_task_runner_.reset(new TaskRunnerImpl());
    MediaPipelineDeviceParams device_params(
        MediaPipelineDeviceParams::kModeIgnorePts, backend_task_runner_.get());
    backend_ = audio_manager->CreateMediaPipelineBackend(device_params);
    if (backend_)
      decoder_ = InitializeBackend(audio_params_, backend_.get(), this);
    *success = decoder_ != nullptr;
    completion_event->Signal();
  }

  void Close() {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (backend_)
      backend_->Stop();
    backend_.reset();
    backend_task_runner_.reset();
  }

  void Start() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(backend_);

    if (first_start_) {
      first_start_ = false;
      backend_->Start(0);
    } else {
      backend_->Resume();
    }
  }

  void Stop() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(backend_);

    backend_->Pause();
  }

  void PushBuffer(scoped_refptr<media::DecoderBufferBase> decoder_buffer,
                  const PushBufferCompletionCallback& completion_cb) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(decoder_);
    DCHECK(!completion_cb.is_null());
    DCHECK(completion_cb_.is_null());
    if (error_) {
      completion_cb.Run(false);
      return;
    }

    backend_buffer_.set_buffer(decoder_buffer);

    MediaPipelineBackend::BufferStatus status =
        decoder_->PushBuffer(nullptr /* decrypt_context */, &backend_buffer_);
    completion_cb_ = completion_cb;
    if (status != MediaPipelineBackend::kBufferPending)
      OnPushBufferComplete(decoder_, status);
  }

  void SetVolume(double volume) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(decoder_);
    decoder_->SetVolume(volume);
  }

  // MediaPipelineBackend::Delegate implementation
  void OnVideoResolutionChanged(MediaPipelineBackend::VideoDecoder* decoder,
                                const Size& size) override {}

  void OnPushBufferComplete(
      MediaPipelineBackend::Decoder* decoder,
      MediaPipelineBackend::BufferStatus status) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK_NE(status, MediaPipelineBackend::kBufferPending);

    base::ResetAndReturn(&completion_cb_)
        .Run(status == MediaPipelineBackend::kBufferSuccess);
  }

  void OnEndOfStream(MediaPipelineBackend::Decoder* decoder) override {}

  void OnDecoderError(MediaPipelineBackend::Decoder* decoder) override {
    error_ = true;
    if (!completion_cb_.is_null())
      OnPushBufferComplete(decoder_, MediaPipelineBackend::kBufferFailed);
  }

 private:
  const ::media::AudioParameters audio_params_;
  scoped_ptr<MediaPipelineBackend> backend_;
  scoped_ptr<TaskRunnerImpl> backend_task_runner_;
  MediaPipelineBackend::AudioDecoder* decoder_;
  PushBufferCompletionCallback completion_cb_;
  bool first_start_;
  bool error_;
  CastDecoderBufferImpl backend_buffer_;
  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(Backend);
};

// CastAudioOutputStream runs on audio thread (AudioManager::GetTaskRunner).
CastAudioOutputStream::CastAudioOutputStream(
    const ::media::AudioParameters& audio_params,
    CastAudioManager* audio_manager)
    : audio_params_(audio_params),
      audio_manager_(audio_manager),
      volume_(1.0),
      source_callback_(nullptr),
      backend_(new Backend(audio_params)),
      buffer_duration_(audio_params.GetBufferDuration()),
      push_in_progress_(false),
      audio_task_runner_(audio_manager->GetTaskRunner()),
      backend_task_runner_(media::MediaMessageLoop::GetTaskRunner()),
      weak_factory_(this) {
  VLOG(1) << "CastAudioOutputStream " << this << " created with "
          << audio_params_.AsHumanReadableString();
}

CastAudioOutputStream::~CastAudioOutputStream() {}

bool CastAudioOutputStream::Open() {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());

  ::media::AudioParameters::Format format = audio_params_.format();
  DCHECK((format == ::media::AudioParameters::AUDIO_PCM_LINEAR) ||
         (format == ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY));

  ::media::ChannelLayout channel_layout = audio_params_.channel_layout();
  if ((channel_layout != ::media::CHANNEL_LAYOUT_MONO) &&
      (channel_layout != ::media::CHANNEL_LAYOUT_STEREO)) {
    LOG(WARNING) << "Unsupported channel layout: " << channel_layout;
    return false;
  }
  DCHECK_GE(audio_params_.channels(), 1);
  DCHECK_LE(audio_params_.channels(), 2);

  {
    // Wait until the backend has initialized so that the outcome can be
    // communicated to the client.
    bool success = false;
    base::WaitableEvent completion_event(false, false);
    backend_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Backend::Open, base::Unretained(backend_.get()),
                              audio_manager_, &success, &completion_event));
    completion_event.Wait();

    if (!success) {
      LOG(WARNING) << "Failed to create media pipeline backend.";
      return false;
    }
  }
  audio_bus_ = ::media::AudioBus::Create(audio_params_);
  decoder_buffer_ = new DecoderBufferAdapter(
      new ::media::DecoderBuffer(audio_params_.GetBytesPerBuffer()));

  VLOG(1) << __FUNCTION__ << " : " << this;
  return true;
}

void CastAudioOutputStream::Close() {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());

  VLOG(1) << __FUNCTION__ << " : " << this;
  backend_task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&Backend::Close, base::Unretained(backend_.get())),
      base::Bind(&CastAudioOutputStream::OnClosed, base::Unretained(this)));
}

void CastAudioOutputStream::Start(AudioSourceCallback* source_callback) {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());
  DCHECK(source_callback);

  source_callback_ = source_callback;
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Backend::Start, base::Unretained(backend_.get())));

  next_push_time_ = base::TimeTicks::Now();
  if (!push_in_progress_) {
    audio_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&CastAudioOutputStream::PushBuffer,
                                            weak_factory_.GetWeakPtr()));
    push_in_progress_ = true;
  }

  metrics::CastMetricsHelper::GetInstance()->LogTimeToFirstAudio();
}

void CastAudioOutputStream::Stop() {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());

  source_callback_ = nullptr;
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Backend::Stop, base::Unretained(backend_.get())));
}

void CastAudioOutputStream::SetVolume(double volume) {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());

  volume_ = volume;
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Backend::SetVolume,
                            base::Unretained(backend_.get()), volume));
}

void CastAudioOutputStream::GetVolume(double* volume) {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());

  *volume = volume_;
}

void CastAudioOutputStream::OnClosed() {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());

  VLOG(1) << __FUNCTION__ << " : " << this;
  // Signal to the manager that we're closed and can be removed.
  // This should be the last call in the function as it deletes "this".
  audio_manager_->ReleaseOutputStream(this);
}

void CastAudioOutputStream::PushBuffer() {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());
  DCHECK(push_in_progress_);

  if (!source_callback_) {
    push_in_progress_ = false;
    return;
  }

  uint32_t bytes_delay = 0;
  int frame_count = source_callback_->OnMoreData(audio_bus_.get(), bytes_delay);
  VLOG(3) << "frames_filled=" << frame_count << " with latency=" << bytes_delay;

  DCHECK_EQ(frame_count, audio_bus_->frames());
  DCHECK_EQ(static_cast<int>(decoder_buffer_->data_size()),
            frame_count * audio_params_.GetBytesPerFrame());
  audio_bus_->ToInterleaved(frame_count, audio_params_.bits_per_sample() / 8,
                            decoder_buffer_->writable_data());

  auto completion_cb = ::media::BindToCurrentLoop(
      base::Bind(&CastAudioOutputStream::OnPushBufferComplete,
                 weak_factory_.GetWeakPtr()));
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&Backend::PushBuffer,
                                            base::Unretained(backend_.get()),
                                            decoder_buffer_,
                                            completion_cb));
}

void CastAudioOutputStream::OnPushBufferComplete(bool success) {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());
  DCHECK(push_in_progress_);

  push_in_progress_ = false;

  if (!source_callback_) {
    return;
  }
  if (!success) {
    source_callback_->OnError(this);
    return;
  }

  // Schedule next push buffer.
  // Need to account for time spent in pulling and pushing buffer as well
  // as the imprecision of PostDelayedTask().
  const base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delay = next_push_time_ + buffer_duration_ - now;
  delay = std::max(delay, base::TimeDelta());
  next_push_time_ = now + delay;

  audio_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CastAudioOutputStream::PushBuffer,
                 weak_factory_.GetWeakPtr()),
      delay);
  push_in_progress_ = true;
}

}  // namespace media
}  // namespace chromecast
