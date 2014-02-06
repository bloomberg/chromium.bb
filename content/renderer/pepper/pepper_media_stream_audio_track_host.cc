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
#include "ppapi/c/ppb_audio_frame.h"
#include "ppapi/shared_impl/media_stream_frame.h"

using media::AudioParameters;

namespace {

// Max audio buffer duration in milliseconds.
const uint32_t kMaxDuration = 10;

// TODO(penghuang): make this configurable.
const int32_t kNumberOfFrames = 4;

}  // namespace

namespace content {

PepperMediaStreamAudioTrackHost::AudioSink::AudioSink(
    PepperMediaStreamAudioTrackHost* host)
    : host_(host),
      frame_data_size_(0),
      main_message_loop_proxy_(base::MessageLoopProxy::current()),
      weak_factory_(this) {
}

PepperMediaStreamAudioTrackHost::AudioSink::~AudioSink() {
  DCHECK_EQ(main_message_loop_proxy_, base::MessageLoopProxy::current());
}

void PepperMediaStreamAudioTrackHost::AudioSink::EnqueueFrame(int32_t index) {
  DCHECK_EQ(main_message_loop_proxy_, base::MessageLoopProxy::current());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, host_->frame_buffer()->number_of_frames());
  base::AutoLock lock(lock_);
  frames_.push_back(index);
}

void PepperMediaStreamAudioTrackHost::AudioSink::InitFramesOnMainThread(
    int32_t number_of_frames, int32_t frame_size) {
  DCHECK_EQ(main_message_loop_proxy_, base::MessageLoopProxy::current());
  bool result = host_->InitFrames(number_of_frames, frame_size);
  // TODO(penghuang): Send PP_ERROR_NOMEMORY to plugin.
  CHECK(result);
  base::AutoLock lock(lock_);
  for (int32_t i = 0; i < number_of_frames; ++i) {
    int32_t index = host_->frame_buffer()->DequeueFrame();
    DCHECK_GE(index, 0);
    frames_.push_back(index);
  }
}

void
PepperMediaStreamAudioTrackHost::AudioSink::SendEnqueueFrameMessageOnMainThread(
    int32_t index) {
  DCHECK_EQ(main_message_loop_proxy_, base::MessageLoopProxy::current());
  host_->SendEnqueueFrameMessageToPlugin(index);
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
    if (!frames_.empty()) {
      index = frames_.front();
      frames_.pop_front();
    }
  }

  if (index != -1) {
    // TODO(penghuang): support re-sampling, etc.
    ppapi::MediaStreamFrame::Audio* frame =
        &(host_->frame_buffer()->GetFramePointer(index)->audio);
    frame->header.size = host_->frame_buffer()->frame_size();
    frame->header.type = ppapi::MediaStreamFrame::TYPE_AUDIO;
    frame->timestamp = timestamp_.InMillisecondsF();
    frame->sample_rate = static_cast<PP_AudioFrame_SampleRate>(sample_rate);
    frame->number_of_channels = number_of_channels;
    frame->number_of_samples = number_of_channels * number_of_frames;
    frame->data_size = frame_data_size_;
    memcpy(frame->data, audio_data, frame_data_size_);

    main_message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&AudioSink::SendEnqueueFrameMessageOnMainThread,
                   weak_factory_.GetWeakPtr(), index));
  }
  timestamp_ += frame_duration_;
}

void PepperMediaStreamAudioTrackHost::AudioSink::OnSetFormat(
    const AudioParameters& params) {
  DCHECK(params.IsValid());
  DCHECK_LE(params.GetBufferDuration().InMilliseconds(), kMaxDuration);
  DCHECK_EQ(params.bits_per_sample(), 16);
  DCHECK((params.sample_rate() == AudioParameters::kTelephoneSampleRate) ||
         (params.sample_rate() == AudioParameters::kAudioCDSampleRate));

  COMPILE_ASSERT(AudioParameters::kTelephoneSampleRate ==
                 static_cast<int32_t>(PP_AUDIOFRAME_SAMPLERATE_8000),
                 audio_sample_rate_does_not_match);
  COMPILE_ASSERT(AudioParameters::kAudioCDSampleRate ==
                 static_cast<int32_t>(PP_AUDIOFRAME_SAMPLERATE_44100),
                 audio_sample_rate_does_not_match);
  audio_params_ = params;

  // TODO(penghuang): support setting format more than once.
  frame_duration_ = params.GetBufferDuration();
  frame_data_size_ = params.GetBytesPerBuffer();

  if (original_audio_params_.IsValid()) {
    DCHECK_EQ(params.sample_rate(), original_audio_params_.sample_rate());
    DCHECK_EQ(params.bits_per_sample(),
              original_audio_params_.bits_per_sample());
    DCHECK_EQ(params.channels(), original_audio_params_.channels());
  } else {
    audio_thread_checker_.DetachFromThread();
    original_audio_params_ = params;
    // The size is slightly bigger than necessary, because 8 extra bytes are
    // added into the struct. Also see |MediaStreamFrame|.
    size_t max_frame_size =
        params.sample_rate() * params.bits_per_sample() / 8 *
        params.channels() * kMaxDuration / 1000;
    size_t size = sizeof(ppapi::MediaStreamFrame::Audio) + max_frame_size;

    main_message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&AudioSink::InitFramesOnMainThread,
                   weak_factory_.GetWeakPtr(),
                   kNumberOfFrames,
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

void PepperMediaStreamAudioTrackHost::OnNewFrameEnqueued() {
  int32_t index = frame_buffer()->DequeueFrame();
  DCHECK_GE(index, 0);
  audio_sink_.EnqueueFrame(index);
}

void PepperMediaStreamAudioTrackHost::DidConnectPendingHostToResource() {
  if (!connected_) {
    MediaStreamAudioSink::AddToAudioTrack(&audio_sink_, track_);
    connected_ = true;
  }
}

}  // namespace content
