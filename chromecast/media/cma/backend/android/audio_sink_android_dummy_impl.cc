// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/android/audio_sink_android_dummy_impl.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "media/base/audio_bus.h"

namespace chromecast {
namespace media {

namespace {

std::string AudioContentTypeToString(media::AudioContentType type) {
  switch (type) {
    case media::AudioContentType::kAlarm:
      return "alarm";
    case media::AudioContentType::kCommunication:
      return "communication";
    default:
      return "media";
  }
}

}  // namespace

AudioSinkAndroidDummyImpl::AudioSinkAndroidDummyImpl(
    AudioSinkAndroid::Delegate* delegate,
    int input_samples_per_second,
    bool primary,
    const std::string& device_id,
    AudioContentType content_type)
    : delegate_(delegate),
      input_samples_per_second_(input_samples_per_second),
      primary_(primary),
      device_id_(device_id),
      content_type_(content_type),
      caller_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      state_(kStateUninitialized),
      stream_volume_multiplier_(1.0f),
      weak_factory_(this) {
  LOG(INFO) << __func__ << "(" << this << "):"
            << " input_samples_per_second_=" << input_samples_per_second_
            << " primary_=" << primary_ << " device_id_=" << device_id_
            << " content_type_=" << AudioContentTypeToString(content_type_);
  DCHECK(delegate_);
  weak_this_ = weak_factory_.GetWeakPtr();
}

AudioSinkAndroidDummyImpl::~AudioSinkAndroidDummyImpl() {
  LOG(INFO) << __func__ << "(" << this << "):";
}

bool AudioSinkAndroidDummyImpl::IsDeleting() const {
  return (state_ == kStateDeleted);
}

void AudioSinkAndroidDummyImpl::PreventDelegateCalls() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  weak_factory_.InvalidateWeakPtrs();
  // do something?
}

void AudioSinkAndroidDummyImpl::WritePcm(
    const scoped_refptr<DecoderBufferBase>& data) {
  if (!data->end_of_stream()) {
    VLOG(3) << __func__ << "(" << this << "):"
            << " data_size=" << data->data_size()
            << " ts=" << data->timestamp();
  } else {
    VLOG(3) << __func__ << "(" << this << "): EOS";
    state_ = kStateGotEos;
  }
  MediaPipelineBackendAndroid::RenderingDelay delay;
  PostPcmCallback(delay);
}

void AudioSinkAndroidDummyImpl::PostPcmCallback(
    const MediaPipelineBackendAndroid::RenderingDelay& delay) {
  VLOG(3) << __func__ << "(" << this << "):"
          << " delay=" << delay.delay_microseconds
          << " ts=" << delay.timestamp_microseconds;
  delegate_->OnWritePcmCompletion(MediaPipelineBackendAndroid::kBufferSuccess,
                                  delay);
}

void AudioSinkAndroidDummyImpl::SetPaused(bool paused) {
  DCHECK(!IsDeleting());
  if (paused) {
    LOG(INFO) << __func__ << "(" << this << "): Pausing";
    state_ = kStatePaused;
  } else {
    LOG(INFO) << __func__ << "(" << this << "): Unpausing";
    state_ = kStateNormalPlayback;
  }
}

void AudioSinkAndroidDummyImpl::SetVolumeMultiplier(float multiplier) {
  DCHECK(!IsDeleting());
  stream_volume_multiplier_ = std::max(0.0f, std::min(multiplier, 1.0f));
  LOG(INFO) << __func__ << "(" << this << "):"
            << " multplier=" << multiplier
            << " stream_volume_multiplier_=" << stream_volume_multiplier_;
  // do something with multiplier
}

}  // namespace media
}  // namespace chromecast
