// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_local_audio_source_provider.h"

#include <string>

#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/audio/audio_device_factory.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_parameters.h"
#include "third_party/blink/public/platform/web_audio_source_provider_client.h"
#include "third_party/blink/public/web/web_local_frame.h"

using blink::WebVector;

namespace {
static const size_t kMaxNumberOfAudioFifoBuffers = 10;
}

namespace content {

// Size of the buffer that WebAudio processes each time, it is the same value
// as AudioNode::ProcessingSizeInFrames in WebKit.
// static
const size_t WebRtcLocalAudioSourceProvider::kWebAudioRenderBufferSize = 128;

WebRtcLocalAudioSourceProvider::WebRtcLocalAudioSourceProvider(
    const blink::WebMediaStreamTrack& track)
    : is_enabled_(false),
      track_(track),
      track_stopped_(false) {
  // Get the native audio output hardware sample-rate for the sink.
  // We need to check if there is a valid frame since the unittests
  // do not have one and they will inject their own |sink_params_| for testing.
  blink::WebLocalFrame* const web_frame =
      blink::WebLocalFrame::FrameForCurrentContext();
  RenderFrame* const render_frame = RenderFrame::FromWebFrame(web_frame);
  if (render_frame) {
    int sample_rate =
        AudioDeviceFactory::GetOutputDeviceInfo(render_frame->GetRoutingID(),
                                                media::AudioSinkParameters())
            .output_params()
            .sample_rate();
    sink_params_.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                       media::CHANNEL_LAYOUT_STEREO, sample_rate,
                       kWebAudioRenderBufferSize);
  }
  // Connect the source provider to the track as a sink.
  blink::WebMediaStreamAudioSink::AddToAudioTrack(this, track_);
}

WebRtcLocalAudioSourceProvider::~WebRtcLocalAudioSourceProvider() {
  if (audio_converter_.get())
    audio_converter_->RemoveInput(this);

  // If the track is still active, it is necessary to notify the track before
  // the source provider goes away.
  if (!track_stopped_)
    blink::WebMediaStreamAudioSink::RemoveFromAudioTrack(this, track_);
}

void WebRtcLocalAudioSourceProvider::OnSetFormat(
    const media::AudioParameters& params) {
  DCHECK(params.IsValid());

  base::AutoLock auto_lock(lock_);
  DCHECK(sink_params_.IsValid());

  source_params_ = params;
  // Create the audio converter with |disable_fifo| as false so that the
  // converter will request source_params.frames_per_buffer() each time.
  // This will not increase the complexity as there is only one client to
  // the converter.
  audio_converter_.reset(
      new media::AudioConverter(params, sink_params_, false));
  audio_converter_->AddInput(this);
  fifo_.reset(new media::AudioFifo(
      params.channels(),
      kMaxNumberOfAudioFifoBuffers * params.frames_per_buffer()));
}

void WebRtcLocalAudioSourceProvider::OnReadyStateChanged(
      blink::WebMediaStreamSource::ReadyState state) {
  NON_REENTRANT_SCOPE(ready_state_reentrancy_checker_);
  if (state == blink::WebMediaStreamSource::kReadyStateEnded)
    track_stopped_ = true;
}

void WebRtcLocalAudioSourceProvider::OnData(
    const media::AudioBus& audio_bus,
    base::TimeTicks estimated_capture_time) {
  NON_REENTRANT_SCOPE(capture_reentrancy_checker_);
  DCHECK(!estimated_capture_time.is_null());

  base::AutoLock auto_lock(lock_);
  if (!is_enabled_)
    return;

  DCHECK(fifo_.get());
  DCHECK_EQ(audio_bus.channels(), source_params_.channels());
  DCHECK_EQ(audio_bus.frames(), source_params_.frames_per_buffer());

  if (fifo_->frames() + audio_bus.frames() <= fifo_->max_frames()) {
    fifo_->Push(&audio_bus);
  } else {
    // This can happen if the data in FIFO is too slowly consumed or
    // WebAudio stops consuming data.
    DVLOG(3) << "Local source provicer FIFO is full" << fifo_->frames();
  }
}

void WebRtcLocalAudioSourceProvider::SetClient(
    blink::WebAudioSourceProviderClient* client) {
  NOTREACHED();
}

void WebRtcLocalAudioSourceProvider::ProvideInput(
    const WebVector<float*>& audio_data,
    size_t number_of_frames) {
  NON_REENTRANT_SCOPE(provide_input_reentrancy_checker_);
  DCHECK_EQ(number_of_frames, kWebAudioRenderBufferSize);

  if (!output_wrapper_ ||
      static_cast<size_t>(output_wrapper_->channels()) != audio_data.size()) {
    output_wrapper_ = media::AudioBus::CreateWrapper(audio_data.size());
  }

  output_wrapper_->set_frames(number_of_frames);
  for (size_t i = 0; i < audio_data.size(); ++i)
    output_wrapper_->SetChannelData(i, audio_data[i]);

  base::AutoLock auto_lock(lock_);
  if (!audio_converter_)
    return;

  is_enabled_ = true;
  audio_converter_->Convert(output_wrapper_.get());
}

// |lock_| needs to be acquired before this function is called. It's called by
// AudioConverter which in turn is called by the above ProvideInput() function.
// Thus thread safety analysis is disabled here and |lock_| acquire manually
// asserted.
double WebRtcLocalAudioSourceProvider::ProvideInput(media::AudioBus* audio_bus,
                                                    uint32_t frames_delayed)
    NO_THREAD_SAFETY_ANALYSIS {
  lock_.AssertAcquired();
  if (fifo_->frames() >= audio_bus->frames()) {
    fifo_->Consume(audio_bus, 0, audio_bus->frames());
  } else {
    audio_bus->Zero();
    DVLOG(1) << "WARNING: Underrun, FIFO has data " << fifo_->frames()
             << " samples but " << audio_bus->frames()
             << " samples are needed";
  }

  return 1.0;
}

void WebRtcLocalAudioSourceProvider::SetSinkParamsForTesting(
    const media::AudioParameters& sink_params) {
  base::AutoLock auto_lock(lock_);
  sink_params_ = sink_params;
}

}  // namespace content
