// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_device_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/win/windows_version.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/sample_rates.h"

using media::AudioParameters;
using media::ChannelLayout;

namespace content {

WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()
    : ref_count_(0),
      audio_transport_callback_(NULL),
      input_delay_ms_(0),
      output_delay_ms_(0),
      initialized_(false),
      playing_(false),
      recording_(false),
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
int WebRtcAudioDeviceImpl::OnData(const int16* audio_data,
                                  int sample_rate,
                                  int number_of_channels,
                                  int number_of_frames,
                                  const std::vector<int>& channels,
                                  int audio_delay_milliseconds,
                                  int current_volume,
                                  bool need_audio_processing,
                                  bool key_pressed) {
  int total_delay_ms = 0;
  {
    base::AutoLock auto_lock(lock_);
    // Return immediately when not recording or |channels| is empty.
    // See crbug.com/274017: renderer crash dereferencing invalid channels[0].
    if (!recording_ || channels.empty())
      return 0;

    // Store the reported audio delay locally.
    input_delay_ms_ = audio_delay_milliseconds;
    total_delay_ms = input_delay_ms_ + output_delay_ms_;
    DVLOG(2) << "total delay: " << input_delay_ms_ + output_delay_ms_;
  }

  // Write audio samples in blocks of 10 milliseconds to the registered
  // webrtc::AudioTransport sink. Keep writing until our internal byte
  // buffer is empty.
  const int16* audio_buffer = audio_data;
  const int samples_per_10_msec = (sample_rate / 100);
  CHECK_EQ(number_of_frames % samples_per_10_msec, 0);
  int accumulated_audio_samples = 0;
  uint32_t new_volume = 0;
  while (accumulated_audio_samples < number_of_frames) {
    // Deliver 10ms of recorded 16-bit linear PCM audio.
    int new_mic_level = audio_transport_callback_->OnDataAvailable(
        &channels[0],
        channels.size(),
        audio_buffer,
        sample_rate,
        number_of_channels,
        samples_per_10_msec,
        total_delay_ms,
        current_volume,
        key_pressed,
        need_audio_processing);

    accumulated_audio_samples += samples_per_10_msec;
    audio_buffer += samples_per_10_msec * number_of_channels;

    // The latest non-zero new microphone level will be returned.
    if (new_mic_level)
      new_volume = new_mic_level;
  }

  return new_volume;
}

void WebRtcAudioDeviceImpl::OnSetFormat(
    const media::AudioParameters& params) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::OnSetFormat()";
}

void WebRtcAudioDeviceImpl::RenderData(uint8* audio_data,
                                       int number_of_channels,
                                       int number_of_frames,
                                       int audio_delay_milliseconds) {
  DCHECK_LE(number_of_frames, output_buffer_size());
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(audio_transport_callback_);
    // Store the reported audio delay locally.
    output_delay_ms_ = audio_delay_milliseconds;
  }

  const int channels = number_of_channels;
  DCHECK_LE(channels, output_channels());

  int samples_per_sec = output_sample_rate();
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

  DCHECK(!renderer_.get() || !renderer_->IsStarted())
      << "The shared audio renderer shouldn't be running";

  capturers_.clear();

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
  *available = (!capturers_.empty());
  return 0;
}

bool WebRtcAudioDeviceImpl::RecordingIsInitialized() const {
  DVLOG(1) << "WebRtcAudioDeviceImpl::RecordingIsInitialized()";
  DCHECK(thread_checker_.CalledOnValidThread());
  return (!capturers_.empty());
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

  {
    base::AutoLock auto_lock(lock_);
    if (recording_)
      return 0;

    recording_ = true;
  }

  start_capture_time_ = base::Time::Now();

  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopRecording() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StopRecording()";
  {
    base::AutoLock auto_lock(lock_);
    if (!recording_)
      return 0;

    recording_ = false;
  }

  // Add histogram data to be uploaded as part of an UMA logging event.
  // This histogram keeps track of total recording times.
  if (!start_capture_time_.is_null()) {
    base::TimeDelta capture_time = base::Time::Now() - start_capture_time_;
    UMA_HISTOGRAM_LONG_TIMES("WebRTC.AudioCaptureTime", capture_time);
  }

  return 0;
}

bool WebRtcAudioDeviceImpl::Recording() const {
  base::AutoLock auto_lock(lock_);
  return recording_;
}

int32_t WebRtcAudioDeviceImpl::SetMicrophoneVolume(uint32_t volume) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::SetMicrophoneVolume(" << volume << ")";
  DCHECK(initialized_);

  // Only one microphone is supported at the moment, which is represented by
  // the default capturer.
  scoped_refptr<WebRtcAudioCapturer> capturer(GetDefaultCapturer());
  if (!capturer.get())
    return -1;

  capturer->SetVolume(volume);
  return 0;
}

// TODO(henrika): sort out calling thread once we start using this API.
int32_t WebRtcAudioDeviceImpl::MicrophoneVolume(uint32_t* volume) const {
  DVLOG(1) << "WebRtcAudioDeviceImpl::MicrophoneVolume()";
  // We only support one microphone now, which is accessed via the default
  // capturer.
  DCHECK(initialized_);
  scoped_refptr<WebRtcAudioCapturer> capturer(GetDefaultCapturer());
  if (!capturer.get())
    return -1;

  *volume = static_cast<uint32_t>(capturer->Volume());

  return 0;
}

int32_t WebRtcAudioDeviceImpl::MaxMicrophoneVolume(uint32_t* max_volume) const {
  DCHECK(initialized_);
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
  // TODO(xians): These kind of hardware methods do not make much sense since we
  // support multiple sources. Remove or figure out new APIs for such methods.
  scoped_refptr<WebRtcAudioCapturer> capturer(GetDefaultCapturer());
  if (!capturer.get())
    return -1;

  *available = (capturer->audio_parameters().channels() == 2);
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
  // We use the default capturer as the recording sample rate.
  scoped_refptr<WebRtcAudioCapturer> capturer(GetDefaultCapturer());
  if (!capturer.get())
    return -1;

  *samples_per_sec = static_cast<uint32_t>(
      capturer->audio_parameters().sample_rate());
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
  if (renderer_.get())
    return false;

  if (!renderer->Initialize(this))
    return false;

  renderer_ = renderer;
  return true;
}

void WebRtcAudioDeviceImpl::AddAudioCapturer(
    const scoped_refptr<WebRtcAudioCapturer>& capturer) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::AddAudioCapturer()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(capturer.get());

  // We only support one microphone today, which means the list can contain
  // only one capturer with a valid device id.
  DCHECK(capturer->device_id().empty() || !GetDefaultCapturer());
  base::AutoLock auto_lock(lock_);
  capturers_.push_back(capturer);
}

scoped_refptr<WebRtcAudioCapturer>
WebRtcAudioDeviceImpl::GetDefaultCapturer() const {
  base::AutoLock auto_lock(lock_);
  for (CapturerList::const_iterator iter = capturers_.begin();
       iter != capturers_.end(); ++iter) {
    if (!(*iter)->device_id().empty())
      return *iter;
  }

  return NULL;
}

}  // namespace content
