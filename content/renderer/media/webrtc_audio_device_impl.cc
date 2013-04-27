// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_device_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/win/windows_version.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/audio_util.h"
#include "media/audio/sample_rates.h"

using media::AudioParameters;
using media::ChannelLayout;

namespace content {

namespace {

const double kMaxVolumeLevel = 255.0;

}  // namespace

WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()
    : ref_count_(0),
      audio_transport_callback_(NULL),
      input_delay_ms_(0),
      output_delay_ms_(0),
      initialized_(false),
      playing_(false),
      recording_(false),
      agc_is_enabled_(false),
      microphone_volume_(0) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()";
}

WebRtcAudioDeviceImpl::~WebRtcAudioDeviceImpl() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::~WebRtcAudioDeviceImpl()";
  DCHECK(thread_checker_.CalledOnValidThread());
  Terminate();
}

int32_t WebRtcAudioDeviceImpl::AddRef() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::subtle::Barrier_AtomicIncrement(&ref_count_, 1);
}

int32_t WebRtcAudioDeviceImpl::Release() {
  DCHECK(thread_checker_.CalledOnValidThread());
  int ret = base::subtle::Barrier_AtomicIncrement(&ref_count_, -1);
  if (ret == 0) {
    delete this;
  }
  return ret;
}

void WebRtcAudioDeviceImpl::CaptureData(const int16* audio_data,
                                        int number_of_channels,
                                        int number_of_frames,
                                        int audio_delay_milliseconds,
                                        double volume) {
  DCHECK_LE(number_of_frames, input_buffer_size());
#if defined(OS_WIN) || defined(OS_MACOSX)
  DCHECK_LE(volume, 1.0);
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
  // We have a special situation on Linux where the microphone volume can be
  // "higher than maximum". The input volume slider in the sound preference
  // allows the user to set a scaling that is higher than 100%. It means that
  // even if the reported maximum levels is N, the actual microphone level can
  // go up to 1.5x*N and that corresponds to a normalized |volume| of 1.5x.
  DCHECK_LE(volume, 1.6);
#endif

  media::AudioParameters input_audio_parameters;
  int output_delay_ms = 0;
  {
    base::AutoLock auto_lock(lock_);
    if (!recording_)
      return;

    // Take a copy of the input parameters while we are under a lock.
    input_audio_parameters = input_audio_parameters_;

    // Store the reported audio delay locally.
    input_delay_ms_ = audio_delay_milliseconds;
    output_delay_ms = output_delay_ms_;

    // Map internal volume range of [0.0, 1.0] into [0, 255] used by the
    // webrtc::VoiceEngine.
    microphone_volume_ = static_cast<uint32_t>(volume * kMaxVolumeLevel);
  }

  const int channels = number_of_channels;
  DCHECK_LE(channels, input_channels());
  uint32_t new_mic_level = 0;

  int samples_per_sec = input_sample_rate();
  if (samples_per_sec == 44100) {
    // Even if the hardware runs at 44.1kHz, we use 44.0 internally.
    samples_per_sec = 44000;
  }
  const int samples_per_10_msec = (samples_per_sec / 100);
  int bytes_per_sample = input_audio_parameters.bits_per_sample() / 8;
  const int bytes_per_10_msec =
      channels * samples_per_10_msec * bytes_per_sample;
  int accumulated_audio_samples = 0;

  const uint8* audio_byte_buffer = reinterpret_cast<const uint8*>(audio_data);

  // Write audio samples in blocks of 10 milliseconds to the registered
  // webrtc::AudioTransport sink. Keep writing until our internal byte
  // buffer is empty.
  while (accumulated_audio_samples < number_of_frames) {
    // Deliver 10ms of recorded 16-bit linear PCM audio.
    audio_transport_callback_->RecordedDataIsAvailable(
        audio_byte_buffer,
        samples_per_10_msec,
        bytes_per_sample,
        channels,
        samples_per_sec,
        input_delay_ms_ + output_delay_ms,
        0,  // TODO(henrika): |clock_drift| parameter is not utilized today.
        microphone_volume_,
        new_mic_level);

    accumulated_audio_samples += samples_per_10_msec;
    audio_byte_buffer += bytes_per_10_msec;
  }

  // The AGC returns a non-zero microphone level if it has been decided
  // that a new level should be set.
  if (new_mic_level != 0) {
    // Use IPC and set the new level. Note that, it will take some time
    // before the new level is effective due to the IPC scheme.
    // During this time, |current_mic_level| will contain "non-valid" values
    // and it might reduce the AGC performance. Measurements on Windows 7 have
    // shown that we might receive old volume levels for one or two callbacks.
    SetMicrophoneVolume(new_mic_level);
  }
}

void WebRtcAudioDeviceImpl::SetCaptureFormat(
    const media::AudioParameters& params) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::SetCaptureFormat()";
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  input_audio_parameters_ = params;
}

void WebRtcAudioDeviceImpl::RenderData(uint8* audio_data,
                                       int number_of_channels,
                                       int number_of_frames,
                                       int audio_delay_milliseconds) {
  DCHECK_LE(number_of_frames, output_buffer_size());
  {
    base::AutoLock auto_lock(lock_);
    // Store the reported audio delay locally.
    output_delay_ms_ = audio_delay_milliseconds;
  }

  const int channels = number_of_channels;
  DCHECK_LE(channels, output_channels());

  int samples_per_sec = output_sample_rate();
  if (samples_per_sec == 44100) {
    // Even if the hardware runs at 44.1kHz, we use 44.0 internally.
    samples_per_sec = 44000;
  }
  int samples_per_10_msec = (samples_per_sec / 100);
  int bytes_per_sample = output_audio_parameters_.bits_per_sample() / 8;
  const int bytes_per_10_msec =
      channels * samples_per_10_msec * bytes_per_sample;

  uint32_t num_audio_samples = 0;
  int accumulated_audio_samples = 0;

  // Get audio samples in blocks of 10 milliseconds from the registered
  // webrtc::AudioTransport source. Keep reading until our internal buffer
  // is full.
  while (accumulated_audio_samples < number_of_frames) {
    // Get 10ms and append output to temporary byte buffer.
    audio_transport_callback_->NeedMorePlayData(samples_per_10_msec,
                                                bytes_per_sample,
                                                channels,
                                                samples_per_sec,
                                                audio_data,
                                                num_audio_samples);
    accumulated_audio_samples += num_audio_samples;
    audio_data += bytes_per_10_msec;
  }
}

void WebRtcAudioDeviceImpl::SetRenderFormat(const AudioParameters& params) {
  DCHECK(thread_checker_.CalledOnValidThread());
  output_audio_parameters_ = params;
}

void WebRtcAudioDeviceImpl::RemoveAudioRenderer(WebRtcAudioRenderer* renderer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(renderer, renderer_);
  base::AutoLock auto_lock(lock_);
  renderer_ = NULL;
  playing_ = false;
}

int32_t WebRtcAudioDeviceImpl::RegisterAudioCallback(
    webrtc::AudioTransport* audio_callback) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::RegisterAudioCallback()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(audio_transport_callback_ == NULL, audio_callback != NULL);
  audio_transport_callback_ = audio_callback;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::Init() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::Init()";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (initialized_)
    return 0;

  DCHECK(!capturer_);
  capturer_ = WebRtcAudioCapturer::CreateCapturer();
  if (capturer_)
    capturer_->AddCapturerSink(this);

  // We need to return a success to continue the initialization of WebRtc VoE
  // because failure on the capturer_ initialization should not prevent WebRTC
  // from working. See issue http://crbug.com/144421 for details.
  initialized_ = true;

  return 0;
}

int32_t WebRtcAudioDeviceImpl::Terminate() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::Terminate()";
  DCHECK(thread_checker_.CalledOnValidThread());

  // Calling Terminate() multiple times in a row is OK.
  if (!initialized_)
    return 0;

  StopRecording();
  StopPlayout();

  // It is necessary to stop the |renderer_| before going away.
  if (renderer_) {
    renderer_->Stop();
    renderer_ = NULL;
  }

  if (capturer_) {
    // |capturer_| is stopped by the media stream, so do not need to
    // call Stop() here.
    capturer_->RemoveCapturerSink(this);
    capturer_ = NULL;
  }

  initialized_ = false;
  return 0;
}

bool WebRtcAudioDeviceImpl::Initialized() const {
  return initialized_;
}

int32_t WebRtcAudioDeviceImpl::PlayoutIsAvailable(bool* available) {
  *available = initialized_;
  return 0;
}

bool WebRtcAudioDeviceImpl::PlayoutIsInitialized() const {
  return initialized_;
}

int32_t WebRtcAudioDeviceImpl::RecordingIsAvailable(bool* available) {
  *available = (capturer_ != NULL);
  return 0;
}

bool WebRtcAudioDeviceImpl::RecordingIsInitialized() const {
  DVLOG(1) << "WebRtcAudioDeviceImpl::RecordingIsInitialized()";
  DCHECK(thread_checker_.CalledOnValidThread());
  return (capturer_ != NULL);
}

int32_t WebRtcAudioDeviceImpl::StartPlayout() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StartPlayout()";
  LOG_IF(ERROR, !audio_transport_callback_) << "Audio transport is missing";
  {
    base::AutoLock auto_lock(lock_);
    if (!audio_transport_callback_)
      return 0;
  }

  if (playing_) {
    // webrtc::VoiceEngine assumes that it is OK to call Start() twice and
    // that the call is ignored the second time.
    return 0;
  }

  playing_ = true;
  start_render_time_ = base::Time::Now();
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopPlayout() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StopPlayout()";
  if (!playing_) {
    // webrtc::VoiceEngine assumes that it is OK to call Stop() just in case.
    return 0;
  }

  // Add histogram data to be uploaded as part of an UMA logging event.
  // This histogram keeps track of total playout times.
  if (!start_render_time_.is_null()) {
    base::TimeDelta render_time = base::Time::Now() - start_render_time_;
    UMA_HISTOGRAM_LONG_TIMES("WebRTC.AudioRenderTime", render_time);
  }

  playing_ = false;
  return 0;
}

bool WebRtcAudioDeviceImpl::Playing() const {
  return playing_;
}

int32_t WebRtcAudioDeviceImpl::StartRecording() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StartRecording()";
  DCHECK(initialized_);
  LOG_IF(ERROR, !audio_transport_callback_) << "Audio transport is missing";
  if (!audio_transport_callback_) {
    return -1;
  }

  start_capture_time_ = base::Time::Now();

  base::AutoLock auto_lock(lock_);
  recording_ = true;

  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopRecording() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StopRecording()";
  if (!recording_) {
    return 0;
  }

  // Add histogram data to be uploaded as part of an UMA logging event.
  // This histogram keeps track of total recording times.
  if (!start_capture_time_.is_null()) {
    base::TimeDelta capture_time = base::Time::Now() - start_capture_time_;
    UMA_HISTOGRAM_LONG_TIMES("WebRTC.AudioCaptureTime", capture_time);
  }

  base::AutoLock auto_lock(lock_);
  recording_ = false;

  return 0;
}

bool WebRtcAudioDeviceImpl::Recording() const {
  base::AutoLock auto_lock(lock_);
  return recording_;
}

int32_t WebRtcAudioDeviceImpl::SetAGC(bool enable) {
  DVLOG(1) <<  "WebRtcAudioDeviceImpl::SetAGC(enable=" << enable << ")";
  DCHECK(initialized_);

  // Return early if we are not changing the AGC state.
  if (enable == agc_is_enabled_)
    return 0;

  // The current implementation does not support changing the AGC state while
  // recording. Using this approach simplifies the design and it is also
  // inline with the  latest WebRTC standard.
  if (!capturer_ || capturer_->is_recording())
    return -1;

  capturer_->SetAutomaticGainControl(enable);
  agc_is_enabled_ = enable;
  return 0;
}

bool WebRtcAudioDeviceImpl::AGC() const {
  DVLOG(1) << "WebRtcAudioDeviceImpl::AGC()";
  DCHECK(thread_checker_.CalledOnValidThread());
  // To reduce the usage of IPC messages, an internal AGC state is used.
  // TODO(henrika): investigate if there is a need for a "deeper" getter.
  return agc_is_enabled_;
}

int32_t WebRtcAudioDeviceImpl::SetMicrophoneVolume(uint32_t volume) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::SetMicrophoneVolume(" << volume << ")";
  DCHECK(initialized_);
  if (!capturer_)
    return -1;

  if (volume > kMaxVolumeLevel)
    return -1;

  // WebRTC uses a range of [0, 255] to represent the level of the microphone
  // volume. The IPC channel between the renderer and browser process works
  // with doubles in the [0.0, 1.0] range and we have to compensate for that.
  double normalized_volume = static_cast<double>(volume) / kMaxVolumeLevel;
  capturer_->SetVolume(normalized_volume);
  return 0;
}

// TODO(henrika): sort out calling thread once we start using this API.
int32_t WebRtcAudioDeviceImpl::MicrophoneVolume(uint32_t* volume) const {
  DVLOG(1) << "WebRtcAudioDeviceImpl::MicrophoneVolume()";
  // The microphone level is fed to this class using the Capture() callback
  // and cached in the same method, i.e. we don't ask the native audio layer
  // for the actual micropone level here.
  DCHECK(initialized_);
  if (!capturer_)
    return -1;
  base::AutoLock auto_lock(lock_);
  *volume = microphone_volume_;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::MaxMicrophoneVolume(uint32_t* max_volume) const {
  *max_volume = kMaxVolumeLevel;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::MinMicrophoneVolume(uint32_t* min_volume) const {
  *min_volume = 0;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StereoPlayoutIsAvailable(bool* available) const {
  DCHECK(initialized_);
  *available = (output_channels() == 2);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StereoRecordingIsAvailable(
    bool* available) const {
  DCHECK(initialized_);
  if (!capturer_)
    return -1;
  *available = (input_channels() == 2);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::PlayoutDelay(uint16_t* delay_ms) const {
  base::AutoLock auto_lock(lock_);
  *delay_ms = static_cast<uint16_t>(output_delay_ms_);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::RecordingDelay(uint16_t* delay_ms) const {
  base::AutoLock auto_lock(lock_);
  *delay_ms = static_cast<uint16_t>(input_delay_ms_);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::RecordingSampleRate(
    uint32_t* samples_per_sec) const {
  *samples_per_sec = static_cast<uint32_t>(input_sample_rate());
  return 0;
}

int32_t WebRtcAudioDeviceImpl::PlayoutSampleRate(
    uint32_t* samples_per_sec) const {
  *samples_per_sec = static_cast<uint32_t>(output_sample_rate());
  return 0;
}

bool WebRtcAudioDeviceImpl::SetAudioRenderer(WebRtcAudioRenderer* renderer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(renderer);

  base::AutoLock auto_lock(lock_);
  if (renderer_)
    return false;

  if (!renderer->Initialize(this))
    return false;

  renderer_ = renderer;
  return true;
}

}  // namespace content
