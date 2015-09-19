// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_output_stream.h"

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/cma/base/cast_decoder_buffer_impl.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/pipeline/frame_status_cb_impl.h"
#include "chromecast/public/media/audio_pipeline_device.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/decrypt_context.h"
#include "chromecast/public/media/media_clock_device.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "media/audio/fake_audio_worker.h"
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

CastAudioOutputStream::CastAudioOutputStream(
    const ::media::AudioParameters& audio_params,
    CastAudioManager* audio_manager)
    : audio_params_(audio_params),
      audio_manager_(audio_manager),
      volume_(1.0),
      audio_device_busy_(false),
      weak_factory_(this) {
  VLOG(1) << "CastAudioOutputStream " << this << " created with "
          << audio_params_.AsHumanReadableString();
}

CastAudioOutputStream::~CastAudioOutputStream() {}

bool CastAudioOutputStream::Open() {
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

  media_pipeline_backend_ = audio_manager_->CreateMediaPipelineBackend();
  if (!media_pipeline_backend_) {
    LOG(WARNING) << "Failed to create media pipeline backend.";
    return false;
  }

  if (!InitClockDevice(media_pipeline_backend_->GetClock())) {
    LOG(WARNING) << "Failed to initialize clock device.";
    return false;
  }

  if (!InitAudioDevice(audio_params_, media_pipeline_backend_->GetAudio())) {
    LOG(WARNING) << "Failed to initialize audio device.";
    return false;
  }

  audio_bus_ = ::media::AudioBus::Create(audio_params_);
  audio_worker_.reset(new ::media::FakeAudioWorker(
      base::ThreadTaskRunnerHandle::Get(), audio_params_));

  VLOG(1) << __FUNCTION__ << " : " << this;
  return true;
}

void CastAudioOutputStream::Close() {
  VLOG(1) << __FUNCTION__ << " : " << this;

  if (media_pipeline_backend_) {
    media_pipeline_backend_->GetClock()->SetState(MediaClockDevice::kStateIdle);
    media_pipeline_backend_->GetAudio()->SetState(
        AudioPipelineDevice::kStateIdle);
  }

  audio_worker_.reset();
  audio_bus_.reset();
  media_pipeline_backend_.reset();

  // Signal to the manager that we're closed and can be removed.
  // This should be the last call in the function as it deletes "this".
  audio_manager_->ReleaseOutputStream(this);
}

void CastAudioOutputStream::Start(AudioSourceCallback* source_callback) {
  MediaClockDevice* clock_device = media_pipeline_backend_->GetClock();
  DCHECK(clock_device);
  if (!clock_device->SetState(MediaClockDevice::kStateRunning)) {
    LOG(WARNING) << "Failed to run clock device.";
    return;
  }
  clock_device->SetRate(1.0f);

  AudioPipelineDevice* audio_device = media_pipeline_backend_->GetAudio();
  DCHECK(audio_device);
  if (!audio_device->SetState(AudioPipelineDevice::kStateRunning)) {
    LOG(WARNING) << "Failed to run audio device.";
    return;
  }

  VLOG(1) << __FUNCTION__ << " : " << this;
  audio_worker_->Start(base::Bind(&CastAudioOutputStream::PushFrame,
                                  weak_factory_.GetWeakPtr(), source_callback));
}

void CastAudioOutputStream::Stop() {
  VLOG(1) << __FUNCTION__ << " : " << this;

  MediaClockDevice* clock_device = media_pipeline_backend_->GetClock();
  DCHECK(clock_device);
  clock_device->SetState(MediaClockDevice::kStateIdle);
  clock_device->SetRate(0.0f);

  AudioPipelineDevice* audio_device = media_pipeline_backend_->GetAudio();
  DCHECK(audio_device);
  audio_device->SetState(AudioPipelineDevice::kStatePaused);
  audio_worker_->Stop();
  audio_device_busy_ = false;
}

void CastAudioOutputStream::SetVolume(double volume) {
  AudioPipelineDevice* audio_device = media_pipeline_backend_->GetAudio();
  DCHECK(audio_device);
  audio_device->SetStreamVolumeMultiplier(volume);
  volume_ = volume;
}

void CastAudioOutputStream::GetVolume(double* volume) {
  *volume = volume_;
}

void CastAudioOutputStream::PushFrame(AudioSourceCallback* source_callback) {
  DCHECK(source_callback);
  if (audio_device_busy_) {
    // Skip pulling data if audio device is still busy.
    LOG(WARNING) << __FUNCTION__ << " : " << this
                 << " skipped because audio device is busy.";
    return;
  }

  int frame_count = source_callback->OnMoreData(audio_bus_.get(), 0);
  DCHECK_EQ(frame_count, audio_bus_->frames());
  int buffer_size = frame_count * audio_params_.GetBytesPerFrame();
  scoped_refptr<::media::DecoderBuffer> decoder_buffer(
      new ::media::DecoderBuffer(buffer_size));
  audio_bus_->ToInterleaved(frame_count, audio_params_.bits_per_sample() / 8,
                            decoder_buffer->writable_data());

  AudioPipelineDevice* audio_device = media_pipeline_backend_->GetAudio();
  DCHECK(audio_device);
  MediaComponentDevice::FrameStatus status = audio_device->PushFrame(
      nullptr,  // decrypt_context
      new CastDecoderBufferImpl(new DecoderBufferAdapter(decoder_buffer)),
      new media::FrameStatusCBImpl(
          base::Bind(&CastAudioOutputStream::OnPushFrameStatus,
                     weak_factory_.GetWeakPtr(), source_callback)));

  if (status == MediaComponentDevice::kFrameFailed) {
    // Note: We cannot call OnPushFrameError directly because it will lead to
    // a recursive lock, which is not supported by base::Lock.
    // This callback is called with a lock held inside FakeAudioWorker.
    // Calling FakeAudioWorker::Stop will try to acquire the lock again.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&CastAudioOutputStream::OnPushFrameError,
                              weak_factory_.GetWeakPtr(), source_callback));
  } else if (status == MediaComponentDevice::kFramePending) {
    audio_device_busy_ = true;
  }
}

void CastAudioOutputStream::OnPushFrameStatus(
    AudioSourceCallback* source_callback,
    MediaComponentDevice::FrameStatus status) {
  DCHECK(audio_device_busy_);
  audio_device_busy_ = false;

  DCHECK_NE(status, MediaComponentDevice::kFramePending);
  if (status == MediaComponentDevice::kFrameFailed)
    OnPushFrameError(source_callback);
}

void CastAudioOutputStream::OnPushFrameError(
    AudioSourceCallback* source_callback) {
  LOG(WARNING) << __FUNCTION__ << " : " << this;
  // Inform audio source about the error and stop pulling data.
  audio_worker_->Stop();
  source_callback->OnError(this);
}

}  // namespace media
}  // namespace chromecast
