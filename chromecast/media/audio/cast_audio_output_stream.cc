// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_output_stream.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/base/media_message_loop.h"
#include "chromecast/media/cma/base/cast_decoder_buffer_impl.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/pipeline/frame_status_cb_impl.h"
#include "chromecast/public/media/audio_pipeline_device.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/decrypt_context.h"
#include "chromecast/public/media/media_clock_device.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"

namespace chromecast {
namespace media {

namespace {
bool InitClockDevice(MediaClockDevice* clock_device) {
  DCHECK(clock_device);
  DCHECK_EQ(clock_device->GetState(), MediaClockDevice::kStateUninitialized);

  if (!clock_device->SetState(media::MediaClockDevice::kStateIdle))
    return false;

  if (!clock_device->ResetTimeline(0))
    return false;

  if (!clock_device->SetRate(1.0))
    return false;

  return true;
}

bool InitAudioDevice(const ::media::AudioParameters& audio_params,
                     AudioPipelineDevice* audio_device) {
  DCHECK(audio_device);
  DCHECK_EQ(audio_device->GetState(), AudioPipelineDevice::kStateUninitialized);

  AudioConfig audio_config;
  audio_config.codec = kCodecPCM;
  audio_config.sample_format = kSampleFormatS16;
  audio_config.bytes_per_channel = audio_params.bits_per_sample() / 8;
  audio_config.channel_number = audio_params.channels();
  audio_config.samples_per_second = audio_params.sample_rate();
  audio_config.extra_data = nullptr;
  audio_config.extra_data_size = 0;
  audio_config.is_encrypted = false;
  if (!audio_device->SetConfig(audio_config))
    return false;

  if (!audio_device->SetState(AudioPipelineDevice::kStateIdle))
    return false;

  return true;
}
}  // namespace

// Backend represents a MediaPipelineBackend adapter that runs on cast
// media thread (media::MediaMessageLoop::GetTaskRunner).
// It can be created and destroyed on any thread, but all other member functions
// must be called on a single thread.
class CastAudioOutputStream::Backend {
 public:
  typedef base::Callback<void(bool)> PushFrameCompletionCallback;

  Backend(const ::media::AudioParameters& audio_params)
      : audio_params_(audio_params) {
    thread_checker_.DetachFromThread();
  }
  ~Backend() {}

  void Open(CastAudioManager* audio_manager,
            bool* success,
            base::WaitableEvent* completion_event) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(backend_ == nullptr);

    scoped_ptr<MediaPipelineBackend> pipeline_backend =
        audio_manager->CreateMediaPipelineBackend();
    if (pipeline_backend && InitClockDevice(pipeline_backend->GetClock()) &&
        InitAudioDevice(audio_params_, pipeline_backend->GetAudio())) {
      backend_ = pipeline_backend.Pass();
    }
    *success = backend_ != nullptr;
    completion_event->Signal();
  }

  void Close() {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (backend_) {
      backend_->GetClock()->SetState(MediaClockDevice::kStateIdle);
      backend_->GetAudio()->SetState(AudioPipelineDevice::kStateIdle);
    }
    backend_.reset();
  }

  void Start() {
    DCHECK(thread_checker_.CalledOnValidThread());

    MediaClockDevice* clock_device = backend_->GetClock();
    clock_device->SetState(MediaClockDevice::kStateRunning);
    clock_device->SetRate(1.0f);

    AudioPipelineDevice* audio_device = backend_->GetAudio();
    audio_device->SetState(AudioPipelineDevice::kStateRunning);
  }

  void Stop() {
    DCHECK(thread_checker_.CalledOnValidThread());

    MediaClockDevice* clock_device = backend_->GetClock();
    clock_device->SetRate(0.0f);
  }

  void PushFrame(scoped_refptr<media::DecoderBufferBase> decoder_buffer,
                 const PushFrameCompletionCallback& completion_cb) {
    DCHECK(thread_checker_.CalledOnValidThread());

    AudioPipelineDevice* audio_device = backend_->GetAudio();
    MediaComponentDevice::FrameStatus status =
        audio_device->PushFrame(nullptr,  // decrypt_context
                                new CastDecoderBufferImpl(decoder_buffer),
                                new media::FrameStatusCBImpl(base::Bind(
                                    &Backend::OnPushFrameStatus,
                                    base::Unretained(this), completion_cb)));

    if (status != MediaComponentDevice::kFramePending)
      OnPushFrameStatus(completion_cb, status);
  }

  void SetVolume(double volume) {
    DCHECK(thread_checker_.CalledOnValidThread());

    AudioPipelineDevice* audio_device = backend_->GetAudio();
    audio_device->SetStreamVolumeMultiplier(volume);
  }

 private:
  void OnPushFrameStatus(const PushFrameCompletionCallback& completion_cb,
                         MediaComponentDevice::FrameStatus status) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK_NE(status, MediaComponentDevice::kFramePending);

    completion_cb.Run(status == MediaComponentDevice::kFrameSuccess);
  }

  const ::media::AudioParameters audio_params_;
  scoped_ptr<MediaPipelineBackend> backend_;
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
      backend_busy_(false),
      buffer_duration_(audio_params.GetBufferDuration()),
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
  if (!backend_busy_) {
    audio_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&CastAudioOutputStream::PushFrame,
                                            weak_factory_.GetWeakPtr()));
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

void CastAudioOutputStream::PushFrame() {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());
  DCHECK(!backend_busy_);

  if (!source_callback_)
    return;

  uint32_t bytes_delay = 0;
  int frame_count = source_callback_->OnMoreData(audio_bus_.get(), bytes_delay);
  VLOG(3) << "frames_filled=" << frame_count << " with latency=" << bytes_delay;

  DCHECK_EQ(frame_count, audio_bus_->frames());
  DCHECK_EQ(static_cast<int>(decoder_buffer_->data_size()),
            frame_count * audio_params_.GetBytesPerFrame());
  audio_bus_->ToInterleaved(frame_count, audio_params_.bits_per_sample() / 8,
                            decoder_buffer_->writable_data());

  auto completion_cb = ::media::BindToCurrentLoop(base::Bind(
      &CastAudioOutputStream::OnPushFrameComplete, weak_factory_.GetWeakPtr()));
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Backend::PushFrame, base::Unretained(backend_.get()),
                 decoder_buffer_, completion_cb));
  backend_busy_ = true;
}

void CastAudioOutputStream::OnPushFrameComplete(bool success) {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());

  backend_busy_ = false;
  if (!source_callback_)
    return;

  if (!success) {
    source_callback_->OnError(this);
    return;
  }

  // Schedule next push frame.
  // Need to account for time spent in pulling and pushing frame as well
  // as the imprecision of PostDelayedTask().
  const base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delay = next_push_time_ + buffer_duration_ - now;
  delay = std::max(delay, base::TimeDelta());
  next_push_time_ = now + delay;

  audio_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CastAudioOutputStream::PushFrame, weak_factory_.GetWeakPtr()),
      delay);
}

}  // namespace media
}  // namespace chromecast
