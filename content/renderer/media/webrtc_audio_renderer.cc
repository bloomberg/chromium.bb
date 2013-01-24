// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_renderer.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/audio_hardware.h"
#include "content/renderer/media/renderer_audio_output_device.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/audio/audio_util.h"
#include "media/audio/sample_rates.h"
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "media/audio/win/core_audio_util_win.h"
#endif

namespace content {

namespace {

// Supported hardware sample rates for output sides.
#if defined(OS_WIN) || defined(OS_MACOSX)
// media::GetAudioOutputHardwareSampleRate() asks the audio layer
// for its current sample rate (set by the user) on Windows and Mac OS X.
// The listed rates below adds restrictions and Initialize()
// will fail if the user selects any rate outside these ranges.
int kValidOutputRates[] = {96000, 48000, 44100};
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
int kValidOutputRates[] = {48000, 44100};
#elif defined(OS_ANDROID)
// On Android, the most popular sampling rate is 16000.
int kValidOutputRates[] = {48000, 44100, 16000};
#else
int kValidOutputRates[] = {44100};
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
      play_ref_count_(0) {
}

WebRtcAudioRenderer::~WebRtcAudioRenderer() {
  DCHECK_EQ(state_, UNINITIALIZED);
  buffer_.reset();
}

bool WebRtcAudioRenderer::Initialize(WebRtcAudioRendererSource* source) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, UNINITIALIZED);
  DCHECK(source);
  DCHECK(!sink_);
  DCHECK(!source_);

  sink_ = AudioDeviceFactory::NewOutputDevice();
  DCHECK(sink_);

  // Ask the browser for the default audio output hardware sample-rate.
  // This request is based on a synchronous IPC message.
  int sample_rate = GetAudioOutputSampleRate();
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

  media::ChannelLayout channel_layout = media::CHANNEL_LAYOUT_STEREO;

  int buffer_size = 0;

  // Windows
#if defined(OS_WIN)
  // Always use stereo rendering on Windows.
  channel_layout = media::CHANNEL_LAYOUT_STEREO;

  // Render side: AUDIO_PCM_LOW_LATENCY is based on the Core Audio (WASAPI)
  // API which was introduced in Windows Vista. For lower Windows versions,
  // a callback-driven Wave implementation is used instead. An output buffer
  // size of 10ms works well for WASAPI but 30ms is needed for Wave.

  // Use different buffer sizes depending on the current hardware sample rate.
  if (sample_rate == 96000 || sample_rate == 48000) {
    buffer_size = (sample_rate / 100);
  } else {
    // We do run at 44.1kHz at the actual audio layer, but ask for frames
    // at 44.0kHz to ensure that we can feed them to the webrtc::VoiceEngine.
    // TODO(henrika): figure out why we seem to need 20ms here for glitch-
    // free audio.
    buffer_size = 2 * 440;
  }

  // Windows XP and lower can't cope with 10 ms output buffer size.
  // It must be extended to 30 ms (60 ms will be used internally by WaveOut).
  // Note that we can't use media::CoreAudioUtil::IsSupported() here since it
  // tries to load the Audioses.dll and it will always fail in the render
  // process.
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    buffer_size = 3 * buffer_size;
    DLOG(WARNING) << "Extending the output buffer size by a factor of three "
                  << "since Windows XP has been detected.";
  }
#elif defined(OS_MACOSX)
  channel_layout = media::CHANNEL_LAYOUT_MONO;

  // Render side: AUDIO_PCM_LOW_LATENCY on Mac OS X is based on a callback-
  // driven Core Audio implementation. Tests have shown that 10ms is a suitable
  // frame size to use for 96kHz, 48kHz and 44.1kHz.

  // Use different buffer sizes depending on the current hardware sample rate.
  if (sample_rate == 96000 || sample_rate == 48000) {
    buffer_size = (sample_rate / 100);
  } else {
    // We do run at 44.1kHz at the actual audio layer, but ask for frames
    // at 44.0kHz to ensure that we can feed them to the webrtc::VoiceEngine.
    buffer_size = 440;
  }
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
  channel_layout = media::CHANNEL_LAYOUT_MONO;

  // Based on tests using the current ALSA implementation in Chrome, we have
  // found that 10ms buffer size on the output side works fine.
  buffer_size = 480;
#else
  DLOG(ERROR) << "Unsupported platform";
  return false;
#endif

  // Store utilized parameters to ensure that we can check them
  // after a successful initialization.
  params_.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
                sample_rate, 16, buffer_size);

  // Allocate local audio buffers based on the parameters above.
  // It is assumed that each audio sample contains 16 bits and each
  // audio frame contains one or two audio samples depending on the
  // number of channels.
  buffer_.reset(new int16[params_.frames_per_buffer() * params_.channels()]);

  source_ = source;
  source->SetRenderFormat(params_);

  // Configure the audio rendering client and start the rendering.
  sink_->Initialize(params_, this);
  sink_->SetSourceRenderView(source_render_view_id_);
  sink_->Start();

  state_ = PAUSED;

  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioOutputChannelLayout",
                            channel_layout, media::CHANNEL_LAYOUT_MAX);
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioOutputFramesPerBuffer",
                            buffer_size, kUnexpectedAudioBufferSize);
  AddHistogramFramesPerBuffer(buffer_size);

  return true;
}

void WebRtcAudioRenderer::Start() {
  // TODO(xians): refactor to make usage of Start/Stop more symmetric.
  NOTIMPLEMENTED();
}

void WebRtcAudioRenderer::Play() {
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED)
    return;

  DCHECK(play_ref_count_ == 0 || state_ == PLAYING);
  ++play_ref_count_;
  state_ = PLAYING;
}

void WebRtcAudioRenderer::Pause() {
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED)
    return;

  DCHECK_EQ(state_, PLAYING);
  DCHECK_GT(play_ref_count_, 0);
  if (!--play_ref_count_)
    state_ = PAUSED;
}

void WebRtcAudioRenderer::Stop() {
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED)
    return;

  source_->RemoveRenderer(this);
  source_ = NULL;
  sink_->Stop();
  state_ = UNINITIALIZED;
}

void WebRtcAudioRenderer::SetVolume(float volume) {
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
  {
    base::AutoLock auto_lock(lock_);
    if (!source_)
      return 0;
    // We need to keep render data for the |source_| reglardless of |state_|,
    // otherwise the data will be buffered up inside |source_|.
    source_->RenderData(reinterpret_cast<uint8*>(buffer_.get()),
                        audio_bus->channels(), audio_bus->frames(),
                        audio_delay_milliseconds);

    // Return 0 frames to play out silence if |state_| is not PLAYING.
    if (state_ != PLAYING)
      return 0;
  }

  // Deinterleave each channel and convert to 32-bit floating-point
  // with nominal range -1.0 -> +1.0 to match the callback format.
  audio_bus->FromInterleaved(buffer_.get(), audio_bus->frames(),
                             params_.bits_per_sample() / 8);
  return audio_bus->frames();
}

void WebRtcAudioRenderer::OnRenderError() {
  NOTIMPLEMENTED();
  LOG(ERROR) << "OnRenderError()";
}

}  // namespace content
