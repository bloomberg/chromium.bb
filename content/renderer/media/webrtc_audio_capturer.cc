// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_capturer.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/audio_hardware.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/audio/audio_util.h"
#include "media/audio/sample_rates.h"

namespace content {

// Supported hardware sample rates for input and output sides.
#if defined(OS_WIN) || defined(OS_MACOSX)
// media::GetAudioInputHardwareSampleRate() asks the audio layer
// for its current sample rate (set by the user) on Windows and Mac OS X.
// The listed rates below adds restrictions and WebRtcAudioDeviceImpl::Init()
// will fail if the user selects any rate outside these ranges.
static int kValidInputRates[] = {96000, 48000, 44100, 32000, 16000, 8000};
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
static int kValidInputRates[] = {48000, 44100};
#elif defined(OS_ANDROID)
static int kValidInputRates[] = {48000, 44100, 16000};
#else
static int kValidInputRates[] = {44100};
#endif

static int GetBufferSizeForSampleRate(int sample_rate) {
  int buffer_size = 0;
#if defined(OS_WIN) || defined(OS_MACOSX)
  // Use different buffer sizes depending on the current hardware sample rate.
  if (sample_rate == 44100) {
    // We do run at 44.1kHz at the actual audio layer, but ask for frames
    // at 44.0kHz to ensure that we can feed them to the webrtc::VoiceEngine.
    buffer_size = 440;
  } else {
    buffer_size = (sample_rate / 100);
    DCHECK_EQ(buffer_size * 100, sample_rate) <<
        "Sample rate not supported. Should have been caught in Init().";
  }
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
  // Based on tests using the current ALSA implementation in Chrome, we have
  // found that the best combination is 20ms on the input side and 10ms on the
  // output side.
  // TODO(henrika): It might be possible to reduce the input buffer
  // size and reduce the delay even more.
  buffer_size = 2 * sample_rate / 100;
#endif

  return buffer_size;
}

// static
scoped_refptr<WebRtcAudioCapturer> WebRtcAudioCapturer::CreateCapturer() {
  scoped_refptr<WebRtcAudioCapturer> capturer = new  WebRtcAudioCapturer();
  if (capturer->Initialize())
    return capturer;

  return NULL;
}

WebRtcAudioCapturer::WebRtcAudioCapturer()
    : source_(NULL),
      running_(false) {
}

WebRtcAudioCapturer::~WebRtcAudioCapturer() {
  DCHECK(sinks_.empty());
}

void WebRtcAudioCapturer::AddCapturerSink(WebRtcAudioCapturerSink* sink) {
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end());
    sinks_.push_back(sink);
  }

  // Tell the |sink| which format we use.
  sink->SetCaptureFormat(params_);
}

void WebRtcAudioCapturer::RemoveCapturerSink(WebRtcAudioCapturerSink* sink) {
  base::AutoLock auto_lock(lock_);
  for (SinkList::iterator it = sinks_.begin(); it != sinks_.end(); ++it) {
    if (sink == *it) {
      sinks_.erase(it);
      break;
    }
  }
}

void WebRtcAudioCapturer::SetCapturerSource(
    const scoped_refptr<media::AudioCapturerSource>& source) {
  DVLOG(1) <<  "SetCapturerSource()";
  scoped_refptr<media::AudioCapturerSource> old_source;
  {
    base::AutoLock auto_lock(lock_);
    if (source_ == source)
      return;

    source_.swap(old_source);
    source_ = source;
  }

  // Detach the old source from normal recording.
  if (old_source)
    old_source->Stop();

  if (source)
    source->Initialize(params_, this, this);
}

bool WebRtcAudioCapturer::Initialize() {
  // Ask the browser for the default audio input hardware sample-rate.
  // This request is based on a synchronous IPC message.
  // TODO(xians): we should ask for the native sample rate of a specific device.
  int sample_rate = GetAudioInputSampleRate();
  DVLOG(1) << "Audio input hardware sample rate: " << sample_rate;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputSampleRate",
                            sample_rate, media::kUnexpectedAudioSampleRate);

  // Verify that the reported input hardware sample rate is supported
  // on the current platform.
  if (std::find(&kValidInputRates[0],
                &kValidInputRates[0] + arraysize(kValidInputRates),
                sample_rate) ==
          &kValidInputRates[arraysize(kValidInputRates)]) {
    DLOG(ERROR) << sample_rate << " is not a supported input rate.";
    return false;
  }

  // Ask the browser for the default number of audio input channels.
  // This request is based on a synchronous IPC message.
  // TODO(xians): we should ask for the layout of a specific device.
  media::ChannelLayout channel_layout = GetAudioInputChannelLayout();
  DVLOG(1) << "Audio input hardware channels: " << channel_layout;

  media::AudioParameters::Format format =
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY;
  int buffer_size = GetBufferSizeForSampleRate(sample_rate);
  if (!buffer_size) {
    DLOG(ERROR) << "Unsupported platform";
    return false;
  }

  params_.Reset(format, channel_layout, sample_rate, 16, buffer_size);

  buffer_.reset(new int16[params_.frames_per_buffer() * params_.channels()]);

  // Create and configure the default audio capturing source. The |source_|
  // will be overwritten if the client call the source calls
  // SetCapturerSource().
  SetCapturerSource(AudioDeviceFactory::NewInputDevice());

  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputChannelLayout",
                            channel_layout, media::CHANNEL_LAYOUT_MAX);

  return true;
}

void WebRtcAudioCapturer::Start() {
  base::AutoLock auto_lock(lock_);
  if (running_)
    return;

  // What Start() does is supposed to be very light, for example, posting a
  // task to another thread, so it is safe to call Start() under the lock.
  if (source_)
    source_->Start();

  running_ = true;
}

void WebRtcAudioCapturer::Stop() {
  scoped_refptr<media::AudioCapturerSource> source;
  {
    base::AutoLock auto_lock(lock_);
    if (!running_)
      return;

    source = source_;
    running_ = false;
  }

  if (source)
    source->Stop();
}

void WebRtcAudioCapturer::SetVolume(double volume) {
  base::AutoLock auto_lock(lock_);

  if (source_)
    source_->SetVolume(volume);
}

void WebRtcAudioCapturer::SetDevice(int session_id) {
  base::AutoLock auto_lock(lock_);
  if (source_)
    source_->SetDevice(session_id);
}

void WebRtcAudioCapturer::SetAutomaticGainControl(bool enable) {
  base::AutoLock auto_lock(lock_);
  if (source_)
    source_->SetAutomaticGainControl(enable);
}

void WebRtcAudioCapturer::Capture(media::AudioBus* audio_source,
                                  int audio_delay_milliseconds,
                                  double volume) {
  // This callback is driven by AudioInputDevice::AudioThreadCallback if
  // |source_| is AudioInputDevice, otherwise it is driven by client's
  // CaptureCallback.
  SinkList sinks;
  {
    base::AutoLock auto_lock(lock_);
    if (!running_)
      return;

    // Copy the sink list to a local variable.
    sinks = sinks_;
  }

  // Interleave, scale, and clip input to int and store result in
  // a local byte buffer.
  audio_source->ToInterleaved(audio_source->frames(),
                              params_.bits_per_sample() / 8,
                              buffer_.get());

  // Feed the data to the sinks.
  for (SinkList::const_iterator it = sinks.begin();
       it != sinks.end();
       ++it) {
    (*it)->CaptureData(reinterpret_cast<const int16*>(buffer_.get()),
                       audio_source->channels(), audio_source->frames(),
                       audio_delay_milliseconds, volume);
  }
}

void WebRtcAudioCapturer::OnCaptureError() {
  NOTIMPLEMENTED();
}

void WebRtcAudioCapturer::OnDeviceStarted(const std::string& device_id) {
  device_id_ = device_id;
}

void WebRtcAudioCapturer::OnDeviceStopped() {
  NOTIMPLEMENTED();
}

}  // namespace content
