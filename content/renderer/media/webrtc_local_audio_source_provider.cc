// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_local_audio_source_provider.h"

#include "base/logging.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_hardware_config.h"
#include "third_party/WebKit/public/web/WebAudioSourceProviderClient.h"

using blink::WebVector;

namespace content {

static const size_t kMaxNumberOfBuffers = 10;

// Size of the buffer that WebAudio processes each time, it is the same value
// as AudioNode::ProcessingSizeInFrames in WebKit.
// static
const size_t WebRtcLocalAudioSourceProvider::kWebAudioRenderBufferSize = 128;

WebRtcLocalAudioSourceProvider::WebRtcLocalAudioSourceProvider()
    : audio_delay_ms_(0),
      volume_(1),
      key_pressed_(false),
      is_enabled_(false) {
}

WebRtcLocalAudioSourceProvider::~WebRtcLocalAudioSourceProvider() {
  if (audio_converter_.get())
    audio_converter_->RemoveInput(this);
}

void WebRtcLocalAudioSourceProvider::Initialize(
    const media::AudioParameters& source_params) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Use the native audio output hardware sample-rate for the sink.
  if (RenderThreadImpl::current()) {
    media::AudioHardwareConfig* hardware_config =
        RenderThreadImpl::current()->GetAudioHardwareConfig();
    int sample_rate = hardware_config->GetOutputSampleRate();
    sink_params_.Reset(
        source_params.format(), media::CHANNEL_LAYOUT_STEREO, 2, 0,
        sample_rate, source_params.bits_per_sample(),
        kWebAudioRenderBufferSize);
  } else {
    // This happens on unittests which does not have a valid RenderThreadImpl,
    // the unittests should have injected their own |sink_params_| for testing.
    DCHECK(sink_params_.IsValid());
  }

  base::AutoLock auto_lock(lock_);
  source_params_ = source_params;
  // Create the audio converter with |disable_fifo| as false so that the
  // converter will request source_params.frames_per_buffer() each time.
  // This will not increase the complexity as there is only one client to
  // the converter.
  audio_converter_.reset(
      new media::AudioConverter(source_params, sink_params_, false));
  audio_converter_->AddInput(this);
  fifo_.reset(new media::AudioFifo(
      source_params.channels(),
      kMaxNumberOfBuffers * source_params.frames_per_buffer()));
}

void WebRtcLocalAudioSourceProvider::DeliverData(
    media::AudioBus* audio_source,
    int audio_delay_milliseconds,
    int volume,
    bool key_pressed) {
  base::AutoLock auto_lock(lock_);
  if (!is_enabled_)
    return;

  DCHECK(fifo_.get());

  if (fifo_->frames() + audio_source->frames() <= fifo_->max_frames()) {
    fifo_->Push(audio_source);
  } else {
    // This can happen if the data in FIFO is too slowed to be consumed or
    // WebAudio stops consuming data.
    DLOG(WARNING) << "Local source provicer FIFO is full" << fifo_->frames();
  }

  // Cache the values for GetAudioProcessingParams().
  last_fill_ = base::TimeTicks::Now();
  audio_delay_ms_ = audio_delay_milliseconds;
  volume_ = volume;
  key_pressed_ = key_pressed;
}

void WebRtcLocalAudioSourceProvider::GetAudioProcessingParams(
    int* delay_ms, int* volume, bool* key_pressed) {
  int elapsed_ms = 0;
  if (!last_fill_.is_null()) {
    elapsed_ms = static_cast<int>(
        (base::TimeTicks::Now() - last_fill_).InMilliseconds());
  }
  *delay_ms = audio_delay_ms_ + elapsed_ms + static_cast<int>(
      1000 * fifo_->frames() / source_params_.sample_rate() + 0.5);
  *volume = volume_;
  *key_pressed = key_pressed_;
}

void WebRtcLocalAudioSourceProvider::setClient(
    blink::WebAudioSourceProviderClient* client) {
  NOTREACHED();
}

void WebRtcLocalAudioSourceProvider::provideInput(
    const WebVector<float*>& audio_data, size_t number_of_frames) {
  DCHECK_EQ(number_of_frames, kWebAudioRenderBufferSize);
  if (!bus_wrapper_ ||
      static_cast<size_t>(bus_wrapper_->channels()) != audio_data.size()) {
    bus_wrapper_ = media::AudioBus::CreateWrapper(audio_data.size());
  }

  bus_wrapper_->set_frames(number_of_frames);
  for (size_t i = 0; i < audio_data.size(); ++i)
    bus_wrapper_->SetChannelData(i, audio_data[i]);

  base::AutoLock auto_lock(lock_);
  DCHECK(audio_converter_.get());
  DCHECK(fifo_.get());
  is_enabled_ = true;
  audio_converter_->Convert(bus_wrapper_.get());
}

double WebRtcLocalAudioSourceProvider::ProvideInput(
    media::AudioBus* audio_bus, base::TimeDelta buffer_delay) {
  if (fifo_->frames() >= audio_bus->frames()) {
    fifo_->Consume(audio_bus, 0, audio_bus->frames());
  } else {
    audio_bus->Zero();
    if (!last_fill_.is_null()) {
      DLOG(WARNING) << "Underrun, FIFO has data " << fifo_->frames()
                    << " samples but " << audio_bus->frames()
                    << " samples are needed";
    }
  }

  return 1.0;
}

void WebRtcLocalAudioSourceProvider::SetSinkParamsForTesting(
    const media::AudioParameters& sink_params) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sink_params_ = sink_params;
}

}  // namespace content
