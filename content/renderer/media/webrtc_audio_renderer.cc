// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_renderer.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/renderer_audio_output_device.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/sample_rates.h"
#include "media/base/audio_hardware_config.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "media/audio/win/core_audio_util_win.h"
#endif

namespace content {

namespace {

// Supported hardware sample rates for output sides.
#if defined(OS_WIN) || defined(OS_MACOSX)
// AudioHardwareConfig::GetOutputSampleRate() asks the audio layer for its
// current sample rate (set by the user) on Windows and Mac OS X.  The listed
// rates below adds restrictions and Initialize() will fail if the user selects
// any rate outside these ranges.
const int kValidOutputRates[] = {96000, 48000, 44100, 32000, 16000};
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
const int kValidOutputRates[] = {48000, 44100};
#elif defined(OS_ANDROID)
// TODO(leozwang): We want to use native sampling rate on Android to achieve
// low latency, currently 16000 is used to work around audio problem on some
// Android devices.
const int kValidOutputRates[] = {48000, 44100, 16000};
#else
const int kValidOutputRates[] = {44100};
#endif

// TODO(xians): Merge the following code to WebRtcAudioCapturer, or remove.
enum AudioFramesPerBuffer {
  k160,
  k320,
  k440,  // WebRTC works internally with 440 audio frames at 44.1kHz.
  k480,
  k640,
  k880,
  k960,
  k1440,
  k1920,
  kUnexpectedAudioBufferSize  // Must always be last!
};

// Helper method to convert integral values to their respective enum values
// above, or kUnexpectedAudioBufferSize if no match exists.
AudioFramesPerBuffer AsAudioFramesPerBuffer(int frames_per_buffer) {
  switch (frames_per_buffer) {
    case 160: return k160;
    case 320: return k320;
    case 440: return k440;
    case 480: return k480;
    case 640: return k640;
    case 880: return k880;
    case 960: return k960;
    case 1440: return k1440;
    case 1920: return k1920;
  }
  return kUnexpectedAudioBufferSize;
}

void AddHistogramFramesPerBuffer(int param) {
  AudioFramesPerBuffer afpb = AsAudioFramesPerBuffer(param);
  if (afpb != kUnexpectedAudioBufferSize) {
    UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioOutputFramesPerBuffer",
                              afpb, kUnexpectedAudioBufferSize);
  } else {
    // Report unexpected sample rates using a unique histogram name.
    UMA_HISTOGRAM_COUNTS("WebRTC.AudioOutputFramesPerBufferUnexpected", param);
  }
}

}  // namespace

WebRtcAudioRenderer::WebRtcAudioRenderer(int source_render_view_id)
    : state_(UNINITIALIZED),
      source_render_view_id_(source_render_view_id),
      source_(NULL),
      play_ref_count_(0),
      audio_delay_milliseconds_(0),
      frame_duration_milliseconds_(0),
      fifo_io_ratio_(1) {
}

WebRtcAudioRenderer::~WebRtcAudioRenderer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, UNINITIALIZED);
  buffer_.reset();
}

bool WebRtcAudioRenderer::Initialize(WebRtcAudioRendererSource* source) {
  DVLOG(1) << "WebRtcAudioRenderer::Initialize()";
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, UNINITIALIZED);
  DCHECK(source);
  DCHECK(!sink_);
  DCHECK(!source_);

  sink_ = AudioDeviceFactory::NewOutputDevice();
  DCHECK(sink_);

  // Use mono on all platforms but Windows for now.
  // TODO(henrika): Tracking at http://crbug.com/166771.
  media::ChannelLayout channel_layout = media::CHANNEL_LAYOUT_MONO;
#if defined(OS_WIN)
  channel_layout = media::CHANNEL_LAYOUT_STEREO;
#endif

  // Ask the renderer for the default audio output hardware sample-rate.
  media::AudioHardwareConfig* hardware_config =
      RenderThreadImpl::current()->GetAudioHardwareConfig();
  int sample_rate = hardware_config->GetOutputSampleRate();
  DVLOG(1) << "Audio output hardware sample rate: " << sample_rate;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioOutputSampleRate",
                            sample_rate, media::kUnexpectedAudioSampleRate);

  // Verify that the reported output hardware sample rate is supported
  // on the current platform.
  if (std::find(&kValidOutputRates[0],
                &kValidOutputRates[0] + arraysize(kValidOutputRates),
                sample_rate) ==
                    &kValidOutputRates[arraysize(kValidOutputRates)]) {
    DLOG(ERROR) << sample_rate << " is not a supported output rate.";
    return false;
  }

  // Set up audio parameters for the source, i.e., the WebRTC client.
  // The WebRTC client only supports multiples of 10ms as buffer size where
  // 10ms is preferred for lowest possible delay.

  media::AudioParameters source_params;
  int buffer_size = 0;

  if (sample_rate % 8000 == 0) {
    buffer_size = (sample_rate / 100);
  } else if (sample_rate == 44100) {
    // The resampler in WebRTC does not support 441 as input. We hard code
    // the size to 440 (~0.9977ms) instead and rely on the internal jitter
    // buffer in WebRTC to deal with the resulting drift.
    // TODO(henrika): ensure that WebRTC supports 44100Hz and use 441 instead.
    buffer_size = 440;
  } else {
    return false;
  }

  int channels = ChannelLayoutToChannelCount(channel_layout);
  source_params.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      channel_layout, channels, 0,
                      sample_rate, 16, buffer_size);

  // Set up audio parameters for the sink, i.e., the native audio output stream.
  // We strive to open up using native parameters to achieve best possible
  // performance and to ensure that no FIFO is needed on the browser side to
  // match the client request. Any mismatch between the source and the sink is
  // taken care of in this class instead using a pull FIFO.

  media::AudioParameters sink_params;

  buffer_size = hardware_config->GetOutputBufferSize();
  sink_params.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                    channel_layout, channels, 0, sample_rate, 16, buffer_size);

  // Create a FIFO if re-buffering is required to match the source input with
  // the sink request. The source acts as provider here and the sink as
  // consumer.
  if (source_params.frames_per_buffer() != sink_params.frames_per_buffer()) {
    DVLOG(1) << "Rebuffering from " << source_params.frames_per_buffer()
             << " to " << sink_params.frames_per_buffer();
    audio_fifo_.reset(new media::AudioPullFifo(
        source_params.channels(),
        source_params.frames_per_buffer(),
        base::Bind(
            &WebRtcAudioRenderer::SourceCallback,
            base::Unretained(this))));

    // The I/O ratio is used in delay calculations where one scheme is used
    // for |fifo_io_ratio_| > 1 and another scheme for < 1.0.
    fifo_io_ratio_ = static_cast<double>(source_params.frames_per_buffer()) /
        sink_params.frames_per_buffer();
  }

  frame_duration_milliseconds_ = base::Time::kMillisecondsPerSecond /
      static_cast<double>(source_params.sample_rate());

  // Allocate local audio buffers based on the parameters above.
  // It is assumed that each audio sample contains 16 bits and each
  // audio frame contains one or two audio samples depending on the
  // number of channels.
  buffer_.reset(
      new int16[source_params.frames_per_buffer() * source_params.channels()]);

  source_ = source;
  source->SetRenderFormat(source_params);

  // Configure the audio rendering client and start rendering.
  sink_->Initialize(sink_params, this);
  sink_->SetSourceRenderView(source_render_view_id_);
  sink_->Start();

  // User must call Play() before any audio can be heard.
  state_ = PAUSED;

  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioOutputChannelLayout",
                            source_params.channel_layout(),
                            media::CHANNEL_LAYOUT_MAX);
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioOutputFramesPerBuffer",
                            source_params.frames_per_buffer(),
                            kUnexpectedAudioBufferSize);
  AddHistogramFramesPerBuffer(source_params.frames_per_buffer());

  return true;
}

void WebRtcAudioRenderer::Start() {
  // TODO(xians): refactor to make usage of Start/Stop more symmetric.
  NOTIMPLEMENTED();
}

void WebRtcAudioRenderer::Play() {
  DVLOG(1) << "WebRtcAudioRenderer::Play()";
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED)
    return;

  DCHECK(play_ref_count_ == 0 || state_ == PLAYING);
  ++play_ref_count_;
  state_ = PLAYING;

  if (audio_fifo_) {
    audio_delay_milliseconds_ = 0;
    audio_fifo_->Clear();
  }
}

void WebRtcAudioRenderer::Pause() {
  DVLOG(1) << "WebRtcAudioRenderer::Pause()";
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED)
    return;

  DCHECK_EQ(state_, PLAYING);
  DCHECK_GT(play_ref_count_, 0);
  if (!--play_ref_count_)
    state_ = PAUSED;
}

void WebRtcAudioRenderer::Stop() {
  DVLOG(1) << "WebRtcAudioRenderer::Stop()";
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED)
    return;

  source_->RemoveAudioRenderer(this);
  source_ = NULL;
  sink_->Stop();
  state_ = UNINITIALIZED;
}

void WebRtcAudioRenderer::SetVolume(float volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED)
    return;

  sink_->SetVolume(volume);
}

base::TimeDelta WebRtcAudioRenderer::GetCurrentRenderTime() const {
  return base::TimeDelta();
}

bool WebRtcAudioRenderer::IsLocalRenderer() const {
  return false;
}

int WebRtcAudioRenderer::Render(media::AudioBus* audio_bus,
                                int audio_delay_milliseconds) {
  base::AutoLock auto_lock(lock_);
  if (!source_)
    return 0;

  DVLOG(2) << "WebRtcAudioRenderer::Render()";
  DVLOG(2) << "audio_delay_milliseconds: " << audio_delay_milliseconds;

  if (fifo_io_ratio_ > 1.0)
    audio_delay_milliseconds_ += audio_delay_milliseconds;
  else
    audio_delay_milliseconds_ = audio_delay_milliseconds;

  if (audio_fifo_)
    audio_fifo_->Consume(audio_bus, audio_bus->frames());
  else
    SourceCallback(0, audio_bus);

  return (state_ == PLAYING) ? audio_bus->frames() : 0;
}

void WebRtcAudioRenderer::OnRenderError() {
  NOTIMPLEMENTED();
  LOG(ERROR) << "OnRenderError()";
}

// Called by AudioPullFifo when more data is necessary.
void WebRtcAudioRenderer::SourceCallback(
    int fifo_frame_delay, media::AudioBus* audio_bus) {
  DVLOG(2) << "WebRtcAudioRenderer::SourceCallback("
           << fifo_frame_delay << ", "
           << audio_bus->frames() << ")";

  int output_delay_milliseconds = audio_delay_milliseconds_;
  output_delay_milliseconds += frame_duration_milliseconds_ * fifo_frame_delay;
  DVLOG(2) << "output_delay_milliseconds: " << output_delay_milliseconds;

  // We need to keep render data for the |source_| regardless of |state_|,
  // otherwise the data will be buffered up inside |source_|.
  source_->RenderData(reinterpret_cast<uint8*>(buffer_.get()),
                      audio_bus->channels(), audio_bus->frames(),
                      output_delay_milliseconds);

  if (fifo_io_ratio_ > 1.0)
    audio_delay_milliseconds_ = 0;

  // Avoid filling up the audio bus if we are not playing; instead
  // return here and ensure that the returned value in Render() is 0.
  if (state_ != PLAYING) {
    audio_bus->Zero();
    return;
  }

  // De-interleave each channel and convert to 32-bit floating-point
  // with nominal range -1.0 -> +1.0 to match the callback format.
  audio_bus->FromInterleaved(buffer_.get(),
                             audio_bus->frames(),
                             sizeof(buffer_[0]));
}

}  // namespace content
