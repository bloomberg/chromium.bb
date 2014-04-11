// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_media_stream_audio_track_host.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_proxy.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio_buffer.h"
#include "ppapi/shared_impl/media_stream_buffer.h"

using media::AudioParameters;

namespace {

// Max audio buffer duration in milliseconds.
const uint32_t kMaxDuration = 10;

// TODO(penghuang): make this configurable.
const int32_t kNumberOfBuffers = 4;

// Returns true if the |sample_rate| is supported in
// |PP_AudioBuffer_SampleRate|, otherwise false.
PP_AudioBuffer_SampleRate GetPPSampleRate(int sample_rate) {
  switch (sample_rate) {
    case 8000:
      return PP_AUDIOBUFFER_SAMPLERATE_8000;
    case 16000:
      return PP_AUDIOBUFFER_SAMPLERATE_16000;
    case 22050:
      return PP_AUDIOBUFFER_SAMPLERATE_22050;
    case 32000:
      return PP_AUDIOBUFFER_SAMPLERATE_32000;
    case 44100:
      return PP_AUDIOBUFFER_SAMPLERATE_44100;
    case 48000:
      return PP_AUDIOBUFFER_SAMPLERATE_48000;
    case 96000:
      return PP_AUDIOBUFFER_SAMPLERATE_96000;
    case 192000:
      return PP_AUDIOBUFFER_SAMPLERATE_192000;
    default:
      return PP_AUDIOBUFFER_SAMPLERATE_UNKNOWN;
  }
}

}  // namespace

namespace content {

PepperMediaStreamAudioTrackHost::AudioSink::AudioSink(
    PepperMediaStreamAudioTrackHost* host)
    : host_(host),
      buffer_data_size_(0),
      main_message_loop_proxy_(base::MessageLoopProxy::current()),
      weak_factory_(this) {}

PepperMediaStreamAudioTrackHost::AudioSink::~AudioSink() {
  DCHECK_EQ(main_message_loop_proxy_, base::MessageLoopProxy::current());
}

void PepperMediaStreamAudioTrackHost::AudioSink::EnqueueBuffer(int32_t index) {
  DCHECK_EQ(main_message_loop_proxy_, base::MessageLoopProxy::current());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, host_->buffer_manager()->number_of_buffers());
  base::AutoLock lock(lock_);
  buffers_.push_back(index);
}

void PepperMediaStreamAudioTrackHost::AudioSink::InitBuffersOnMainThread(
    int32_t number_of_buffers,
    int32_t buffer_size) {
  DCHECK_EQ(main_message_loop_proxy_, base::MessageLoopProxy::current());
  bool result = host_->InitBuffers(number_of_buffers, buffer_size);
  // TODO(penghuang): Send PP_ERROR_NOMEMORY to plugin.
  CHECK(result);
  base::AutoLock lock(lock_);
  for (int32_t i = 0; i < number_of_buffers; ++i) {
    int32_t index = host_->buffer_manager()->DequeueBuffer();
    DCHECK_GE(index, 0);
    buffers_.push_back(index);
  }
}

void PepperMediaStreamAudioTrackHost::AudioSink::
    SendEnqueueBufferMessageOnMainThread(int32_t index) {
  DCHECK_EQ(main_message_loop_proxy_, base::MessageLoopProxy::current());
  host_->SendEnqueueBufferMessageToPlugin(index);
}

void PepperMediaStreamAudioTrackHost::AudioSink::OnData(const int16* audio_data,
                                                        int sample_rate,
                                                        int number_of_channels,
                                                        int number_of_frames) {
  DCHECK(audio_thread_checker_.CalledOnValidThread());
  DCHECK(audio_data);
  DCHECK_EQ(sample_rate, audio_params_.sample_rate());
  DCHECK_EQ(number_of_channels, audio_params_.channels());
  DCHECK_EQ(number_of_frames, audio_params_.frames_per_buffer());
  int32_t index = -1;
  {
    base::AutoLock lock(lock_);
    if (!buffers_.empty()) {
      index = buffers_.front();
      buffers_.pop_front();
    }
  }

  if (index != -1) {
    // TODO(penghuang): support re-sampling, etc.
    ppapi::MediaStreamBuffer::Audio* buffer =
        &(host_->buffer_manager()->GetBufferPointer(index)->audio);
    buffer->header.size = host_->buffer_manager()->buffer_size();
    buffer->header.type = ppapi::MediaStreamBuffer::TYPE_AUDIO;
    buffer->timestamp = timestamp_.InMillisecondsF();
    buffer->sample_rate = static_cast<PP_AudioBuffer_SampleRate>(sample_rate);
    buffer->number_of_channels = number_of_channels;
    buffer->number_of_samples = number_of_channels * number_of_frames;
    buffer->data_size = buffer_data_size_;
    memcpy(buffer->data, audio_data, buffer_data_size_);

    main_message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&AudioSink::SendEnqueueBufferMessageOnMainThread,
                   weak_factory_.GetWeakPtr(),
                   index));
  }
  timestamp_ += buffer_duration_;
}

void PepperMediaStreamAudioTrackHost::AudioSink::OnSetFormat(
    const AudioParameters& params) {
  DCHECK(params.IsValid());
  DCHECK_LE(params.GetBufferDuration().InMilliseconds(), kMaxDuration);
  DCHECK_EQ(params.bits_per_sample(), 16);
  DCHECK_NE(GetPPSampleRate(params.sample_rate()),
            PP_AUDIOBUFFER_SAMPLERATE_UNKNOWN);

  audio_params_ = params;

  // TODO(penghuang): support setting format more than once.
  buffer_duration_ = params.GetBufferDuration();
  buffer_data_size_ = params.GetBytesPerBuffer();

  if (original_audio_params_.IsValid()) {
    DCHECK_EQ(params.sample_rate(), original_audio_params_.sample_rate());
    DCHECK_EQ(params.bits_per_sample(),
              original_audio_params_.bits_per_sample());
    DCHECK_EQ(params.channels(), original_audio_params_.channels());
  } else {
    audio_thread_checker_.DetachFromThread();
    original_audio_params_ = params;
    // The size is slightly bigger than necessary, because 8 extra bytes are
    // added into the struct. Also see |MediaStreamBuffer|.
    size_t max_data_size = params.sample_rate() * params.bits_per_sample() / 8 *
                           params.channels() * kMaxDuration / 1000;
    size_t size = sizeof(ppapi::MediaStreamBuffer::Audio) + max_data_size;

    main_message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&AudioSink::InitBuffersOnMainThread,
                   weak_factory_.GetWeakPtr(),
                   kNumberOfBuffers,
                   static_cast<int32_t>(size)));
  }
}

PepperMediaStreamAudioTrackHost::PepperMediaStreamAudioTrackHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    const blink::WebMediaStreamTrack& track)
    : PepperMediaStreamTrackHostBase(host, instance, resource),
      track_(track),
      connected_(false),
      audio_sink_(this) {
  DCHECK(!track_.isNull());
}

PepperMediaStreamAudioTrackHost::~PepperMediaStreamAudioTrackHost() {
  OnClose();
}

void PepperMediaStreamAudioTrackHost::OnClose() {
  if (connected_) {
    MediaStreamAudioSink::RemoveFromAudioTrack(&audio_sink_, track_);
    connected_ = false;
  }
}

void PepperMediaStreamAudioTrackHost::OnNewBufferEnqueued() {
  int32_t index = buffer_manager()->DequeueBuffer();
  DCHECK_GE(index, 0);
  audio_sink_.EnqueueBuffer(index);
}

void PepperMediaStreamAudioTrackHost::DidConnectPendingHostToResource() {
  if (!connected_) {
    MediaStreamAudioSink::AddToAudioTrack(&audio_sink_, track_);
    connected_ = true;
  }
}

}  // namespace content
