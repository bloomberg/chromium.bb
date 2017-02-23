// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/silent_sink_suspender.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebAudioDevice;
using blink::WebAudioLatencyHint;
using blink::WebLocalFrame;
using blink::WebVector;
using blink::WebView;

namespace content {

namespace {

AudioDeviceFactory::SourceType GetLatencyHintSourceType(
    WebAudioLatencyHint::AudioContextLatencyCategory latency_category) {
  switch (latency_category) {
    case WebAudioLatencyHint::kCategoryInteractive:
      return AudioDeviceFactory::kSourceWebAudioInteractive;
    case WebAudioLatencyHint::kCategoryBalanced:
      return AudioDeviceFactory::kSourceWebAudioBalanced;
    case WebAudioLatencyHint::kCategoryPlayback:
      return AudioDeviceFactory::kSourceWebAudioPlayback;
    case WebAudioLatencyHint::kCategoryExact:
      // TODO implement CategoryExact
      return AudioDeviceFactory::kSourceWebAudioInteractive;
  }
  NOTREACHED();
  return AudioDeviceFactory::kSourceWebAudioInteractive;
}

int FrameIdFromCurrentContext() {
  // Assumption: This method is being invoked within a V8 call stack.  CHECKs
  // will fail in the call to frameForCurrentContext() otherwise.
  //
  // Therefore, we can perform look-ups to determine which RenderView is
  // starting the audio device.  The reason for all this is because the creator
  // of the WebAudio objects might not be the actual source of the audio (e.g.,
  // an extension creates a object that is passed and used within a page).
  blink::WebLocalFrame* const web_frame =
      blink::WebLocalFrame::frameForCurrentContext();
  RenderFrame* const render_frame = RenderFrame::FromWebFrame(web_frame);
  return render_frame ? render_frame->GetRoutingID() : MSG_ROUTING_NONE;
}

media::AudioParameters GetOutputDeviceParameters(
    int frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  return AudioDeviceFactory::GetOutputDeviceInfo(frame_id, session_id,
                                                 device_id, security_origin)
      .output_params();
}

}  // namespace

RendererWebAudioDeviceImpl* RendererWebAudioDeviceImpl::Create(
    media::ChannelLayout layout,
    int channels,
    const blink::WebAudioLatencyHint& latency_hint,
    WebAudioDevice::RenderCallback* callback,
    int session_id,
    const url::Origin& security_origin) {
  return new RendererWebAudioDeviceImpl(layout, channels, latency_hint,
                                        callback, session_id, security_origin,
                                        base::Bind(&GetOutputDeviceParameters),
                                        base::Bind(&FrameIdFromCurrentContext));
}

RendererWebAudioDeviceImpl::RendererWebAudioDeviceImpl(
    media::ChannelLayout layout,
    int channels,
    const blink::WebAudioLatencyHint& latency_hint,
    WebAudioDevice::RenderCallback* callback,
    int session_id,
    const url::Origin& security_origin,
    const OutputDeviceParamsCallback& device_params_cb,
    const RenderFrameIdCallback& render_frame_id_cb)
    : latency_hint_(latency_hint),
      client_callback_(callback),
      session_id_(session_id),
      security_origin_(security_origin),
      frame_id_(render_frame_id_cb.Run()) {
  DCHECK(client_callback_);
  DCHECK_NE(frame_id_, MSG_ROUTING_NONE);

  media::AudioParameters hardware_params(device_params_cb.Run(
      frame_id_, session_id_, std::string(), security_origin_));

  int output_buffer_size = 0;

  media::AudioLatency::LatencyType latency =
      AudioDeviceFactory::GetSourceLatencyType(
          GetLatencyHintSourceType(latency_hint_.category()));

  // Adjust output buffer size according to the latency requirement.
  switch (latency) {
    case media::AudioLatency::LATENCY_INTERACTIVE:
      output_buffer_size = media::AudioLatency::GetInteractiveBufferSize(
          hardware_params.frames_per_buffer());
      break;
    case media::AudioLatency::LATENCY_RTC:
      output_buffer_size = media::AudioLatency::GetRtcBufferSize(
          hardware_params.sample_rate(), hardware_params.frames_per_buffer());
      break;
    case media::AudioLatency::LATENCY_PLAYBACK:
      output_buffer_size = media::AudioLatency::GetHighLatencyBufferSize(
          hardware_params.sample_rate(), 0);
      break;
    case media::AudioLatency::LATENCY_EXACT_MS:
    // TODO(olka): add support when WebAudio requires it.
    default:
      NOTREACHED();
  }

  DCHECK_NE(output_buffer_size, 0);

  sink_params_.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY, layout,
                     hardware_params.sample_rate(), 16, output_buffer_size);
  // Always set channels, this should be a no-op in all but the discrete case;
  // this call will fail if channels doesn't match the layout in other cases.
  sink_params_.set_channels_for_discrete(channels);

  // Specify the latency info to be passed to the browser side.
  sink_params_.set_latency_tag(latency);
}

RendererWebAudioDeviceImpl::~RendererWebAudioDeviceImpl() {
  DCHECK(!sink_);
}

void RendererWebAudioDeviceImpl::start() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (sink_)
    return;  // Already started.

  sink_ = AudioDeviceFactory::NewAudioRendererSink(
      GetLatencyHintSourceType(latency_hint_.category()), frame_id_,
      session_id_, std::string(), security_origin_);

#if defined(OS_ANDROID)
  // Use the media thread instead of the render thread for fake Render() calls
  // since it has special connotations for Blink and garbage collection. Timeout
  // value chosen to be highly unlikely in the normal case.
  webaudio_suspender_.reset(new media::SilentSinkSuspender(
      this, base::TimeDelta::FromSeconds(30), sink_params_, sink_,
      GetMediaTaskRunner()));
  sink_->Initialize(sink_params_, webaudio_suspender_.get());
#else
  sink_->Initialize(sink_params_, this);
#endif

  sink_->Start();
  sink_->Play();
}

void RendererWebAudioDeviceImpl::stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (sink_) {
    sink_->Stop();
    sink_ = nullptr;
  }

#if defined(OS_ANDROID)
  webaudio_suspender_.reset();
#endif
}

double RendererWebAudioDeviceImpl::sampleRate() {
  return sink_params_.sample_rate();
}

int RendererWebAudioDeviceImpl::framesPerBuffer() {
  return sink_params_.frames_per_buffer();
}

int RendererWebAudioDeviceImpl::Render(base::TimeDelta delay,
                                       base::TimeTicks delay_timestamp,
                                       int prior_frames_skipped,
                                       media::AudioBus* dest) {
  // Wrap the output pointers using WebVector.
  WebVector<float*> web_audio_dest_data(static_cast<size_t>(dest->channels()));
  for (int i = 0; i < dest->channels(); ++i)
    web_audio_dest_data[i] = dest->channel(i);

  if (!delay.is_zero()) {  // Zero values are send at the first call.
    // Substruct the bus duration to get hardware delay.
    delay -=
        media::AudioTimestampHelper::FramesToTime(dest->frames(), sampleRate());
  }
  DCHECK_GE(delay, base::TimeDelta());

  client_callback_->render(
      web_audio_dest_data, dest->frames(), delay.InSecondsF(),
      (delay_timestamp - base::TimeTicks()).InSecondsF(), prior_frames_skipped);

  return dest->frames();
}

void RendererWebAudioDeviceImpl::OnRenderError() {
  // TODO(crogers): implement error handling.
}

void RendererWebAudioDeviceImpl::SetMediaTaskRunnerForTesting(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner) {
  media_task_runner_ = media_task_runner;
}

const scoped_refptr<base::SingleThreadTaskRunner>&
RendererWebAudioDeviceImpl::GetMediaTaskRunner() {
  if (!media_task_runner_) {
    media_task_runner_ =
        RenderThreadImpl::current()->GetMediaThreadTaskRunner();
  }
  return media_task_runner_;
}

}  // namespace content
