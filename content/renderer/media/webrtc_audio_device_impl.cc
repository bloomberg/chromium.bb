// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_device_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/win/windows_version.h"
#include "content/renderer/media/audio_hardware.h"
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

const int64 kMillisecondsBetweenProcessCalls = 5000;
const double kMaxVolumeLevel = 255.0;

}  // namespace

WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()
    : ref_count_(0),
      render_loop_(base::MessageLoopProxy::current()),
      audio_transport_callback_(NULL),
      input_delay_ms_(0),
      output_delay_ms_(0),
      last_error_(AudioDeviceModule::kAdmErrNone),
      last_process_time_(base::TimeTicks::Now()),
      initialized_(false),
      playing_(false),
      agc_is_enabled_(false) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()";
}

WebRtcAudioDeviceImpl::~WebRtcAudioDeviceImpl() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::~WebRtcAudioDeviceImpl()";
  Terminate();
}

int32_t WebRtcAudioDeviceImpl::AddRef() {
  return base::subtle::Barrier_AtomicIncrement(&ref_count_, 1);
}

int32_t WebRtcAudioDeviceImpl::Release() {
  int ret = base::subtle::Barrier_AtomicIncrement(&ref_count_, -1);
  if (ret == 0) {
    delete this;
  }
  return ret;
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
  output_audio_parameters_ = params;
}

void WebRtcAudioDeviceImpl::RemoveRenderer(WebRtcAudioRenderer* renderer) {
  DCHECK(renderer);
  base::AutoLock auto_lock(lock_);
  if (renderer != renderer_)
    return;

  renderer_ = NULL;
  playing_ = false;
}

// TODO(xians): Change the name to SetAudioRenderer().
bool WebRtcAudioDeviceImpl::SetRenderer(WebRtcAudioRenderer* renderer) {
  DCHECK(renderer);

  base::AutoLock auto_lock(lock_);
  if (renderer_)
    return false;

  if (!renderer->Initialize(this))
    return false;

  renderer_ = renderer;
  return true;
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
  // go up to 1.5*N and that corresponds to a normalized |volume| of 1.5.
  DCHECK_LE(volume, 1.5);
#endif

  int output_delay_ms = 0;
  {
    base::AutoLock auto_lock(lock_);
    // Store the reported audio delay locally.
    input_delay_ms_ = audio_delay_milliseconds;
    output_delay_ms = output_delay_ms_;
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
  int bytes_per_sample = input_audio_parameters_.bits_per_sample() / 8;
  const int bytes_per_10_msec =
      channels * samples_per_10_msec * bytes_per_sample;
  int accumulated_audio_samples = 0;

  const uint8* audio_byte_buffer = reinterpret_cast<const uint8*>(audio_data);

  // Map internal volume range of [0.0, 1.0] into [0, 255] used by the
  // webrtc::VoiceEngine.
  uint32_t current_mic_level = static_cast<uint32_t>(volume * kMaxVolumeLevel);

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
        current_mic_level,
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
  input_audio_parameters_ = params;
}

int32_t WebRtcAudioDeviceImpl::ChangeUniqueId(const int32_t id) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::TimeUntilNextProcess() {
  // Calculate the number of milliseconds until this module wants its
  // Process method to be called.
  base::TimeDelta delta_time = (base::TimeTicks::Now() - last_process_time_);
  int64 time_until_next =
      kMillisecondsBetweenProcessCalls - delta_time.InMilliseconds();
  return static_cast<int32_t>(time_until_next);
}

int32_t WebRtcAudioDeviceImpl::Process() {
  // TODO(henrika): it is possible to add functionality in this method, which
  // is called periodically. The idea is that we should call one of the methods
  // in the registered AudioDeviceObserver to inform the user about warnings
  // or error states. Leave it empty for now.
  last_process_time_ = base::TimeTicks::Now();
  return 0;
}

int32_t WebRtcAudioDeviceImpl::ActiveAudioLayer(AudioLayer* audio_layer) const {
  NOTIMPLEMENTED();
  return -1;
}

webrtc::AudioDeviceModule::ErrorCode WebRtcAudioDeviceImpl::LastError() const {
  return last_error_;
}

int32_t WebRtcAudioDeviceImpl::RegisterEventObserver(
    webrtc::AudioDeviceObserver* event_callback) {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::RegisterEventObserver() "
           << "NOT IMPLEMENTED";
  return -1;
}

int32_t WebRtcAudioDeviceImpl::RegisterAudioCallback(
    webrtc::AudioTransport* audio_callback) {
  DCHECK_EQ(audio_transport_callback_ == NULL, audio_callback != NULL);
  audio_transport_callback_ = audio_callback;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::Init() {
  DVLOG(1) << "Init()";

  // TODO(henrika): Remove the following code if the |capturer_| does not need
  // to be initialized in the render thread any more.
  if (!render_loop_->BelongsToCurrentThread()) {
    int32_t error = 0;
    base::WaitableEvent event(false, false);
    // Ensure that we call Init() from the main render thread since
    // the audio clients can only be created on this thread.
    render_loop_->PostTask(
        FROM_HERE,
        base::Bind(&WebRtcAudioDeviceImpl::InitOnRenderThread,
                   this, &error, &event));
    event.Wait();
    return error;
  }

  // Calling Init() multiple times in a row is OK.
  // TODO(henrika): Figure out why we need to call Init()/Terminate() for
  // multiple times. This feels like a bug on the webrtc side if Init is called
  // multiple times. Init() in my mind feels like a constructor, so unless
  // Terminate() is called (the equivalent of a dtor), then Init() should not
  // be called randomly after it has already been called.
  if (initialized_)
    return 0;

  DCHECK(!capturer_);
  capturer_ = WebRtcAudioCapturer::CreateCapturer();
  if (capturer_)
    capturer_->AddCapturerSink(this);

  // We need to return a success to continue the initialization of WebRtc VoE
  // because failure on the capturer_ initialization should not prevent WebRTC
  // from working. See issue 144421 for details.
  initialized_ = true;

  return 0;
}

void WebRtcAudioDeviceImpl::InitOnRenderThread(int32_t* error,
                                               base::WaitableEvent* event) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  *error = Init();
  event->Signal();
}

int32_t WebRtcAudioDeviceImpl::Terminate() {
  DVLOG(1) << "Terminate()";

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

  // Release all resources allocated in Init().
  if (capturer_) {
    capturer_->RemoveCapturerSink(this);
    capturer_ = NULL;
  }

  initialized_ = false;
  return 0;
}

bool WebRtcAudioDeviceImpl::Initialized() const {
  return initialized_;
}

int16_t WebRtcAudioDeviceImpl::PlayoutDevices() {
  NOTIMPLEMENTED();
  return -1;
}

int16_t WebRtcAudioDeviceImpl::RecordingDevices() {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::PlayoutDeviceName(
    uint16_t index,
    char name[webrtc::kAdmMaxDeviceNameSize],
    char guid[webrtc::kAdmMaxGuidSize]) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::RecordingDeviceName(
    uint16_t index,
    char name[webrtc::kAdmMaxDeviceNameSize],
    char guid[webrtc::kAdmMaxGuidSize]) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SetPlayoutDevice(uint16_t index) {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetPlayoutDevice() "
           << "NOT IMPLEMENTED";
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetPlayoutDevice(WindowsDeviceType device) {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetPlayoutDevice() "
           << "NOT IMPLEMENTED";
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetRecordingDevice(uint16_t index) {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetRecordingDevice() "
           << "NOT IMPLEMENTED";
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetRecordingDevice(WindowsDeviceType device) {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetRecordingDevice() "
           << "NOT IMPLEMENTED";
  return 0;
}

int32_t WebRtcAudioDeviceImpl::PlayoutIsAvailable(bool* available) {
  DVLOG(1) << "PlayoutIsAvailable()";
  *available = initialized_;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::InitPlayout() {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::InitPlayout() "
           << "NOT IMPLEMENTED";
  return 0;
}

bool WebRtcAudioDeviceImpl::PlayoutIsInitialized() const {
  DVLOG(1) << "PlayoutIsInitialized()";
  return initialized_;
}

int32_t WebRtcAudioDeviceImpl::RecordingIsAvailable(bool* available) {
  DVLOG(1) << "RecordingIsAvailable()";
  *available = (capturer_ != NULL);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::InitRecording() {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::InitRecording() "
           << "NOT IMPLEMENTED";
  return 0;
}

bool WebRtcAudioDeviceImpl::RecordingIsInitialized() const {
  DVLOG(1) << "RecordingIsInitialized()";
  return (capturer_ != NULL);
}

int32_t WebRtcAudioDeviceImpl::StartPlayout() {
  DVLOG(1) << "StartPlayout()";
  LOG_IF(ERROR, !audio_transport_callback_) << "Audio transport is missing";
  {
    base::AutoLock auto_lock(lock_);
    if (!audio_transport_callback_)
      return -1;
  }

  if (playing_) {
    // webrtc::VoiceEngine assumes that it is OK to call Start() twice and
    // that the call is ignored the second time.
    return 0;
  }

  {
    // TODO(xians): We're calling out while under a lock.  This opens up
    // possibilities for deadlocks.  Instead lock, transfer ownership of the
    // renderer_ pointer over to a local variable, release the lock, and call
    // Play() using the local variable.
    base::AutoLock auto_lock(lock_);
    if (renderer_)
      renderer_->Play();
  }

  playing_ = true;
  start_render_time_ = base::Time::Now();
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopPlayout() {
  DVLOG(1) << "StopPlayout()";
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

  {
    base::AutoLock auto_lock(lock_);
    // TODO(xians): transfer ownership of the renderer_ pointer over to a local
    // variable, release the lock, and call Pause() using the local variable.
    if (renderer_)
      renderer_->Pause();
  }

  playing_ = false;
  return 0;
}

bool WebRtcAudioDeviceImpl::Playing() const {
  return playing_;
}

int32_t WebRtcAudioDeviceImpl::StartRecording() {
  DCHECK(initialized_);
  DVLOG(1) << "StartRecording()";
  LOG_IF(ERROR, !audio_transport_callback_) << "Audio transport is missing";
  if (!audio_transport_callback_ || !capturer_) {
    return -1;
  }

  if (capturer_->is_recording()) {
    // webrtc::VoiceEngine assumes that it is OK to call Start() twice and
    // that the call is ignored the second time.
    return 0;
  }

  start_capture_time_ = base::Time::Now();

  // Start capturing using the selected device.
  capturer_->Start();
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopRecording() {
  DVLOG(1) << "StopRecording()";
  if (!capturer_ || !capturer_->is_recording()) {
    // webrtc::VoiceEngine assumes that it is OK to call Stop()
    // more than once.
    return 0;
  }

  // Add histogram data to be uploaded as part of an UMA logging event.
  // This histogram keeps track of total recording times.
  if (!start_capture_time_.is_null()) {
    base::TimeDelta capture_time = base::Time::Now() - start_capture_time_;
    UMA_HISTOGRAM_LONG_TIMES("WebRTC.AudioCaptureTime", capture_time);
  }

  capturer_->Stop();

  return 0;
}

bool WebRtcAudioDeviceImpl::Recording() const {
  if (capturer_)
    return capturer_->is_recording();

  return false;
}

int32_t WebRtcAudioDeviceImpl::SetAGC(bool enable) {
  DCHECK(initialized_);
  DVLOG(1) <<  "SetAGC(enable=" << enable << ")";
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
  // To reduce the usage of IPC messages, an internal AGC state is used.
  // TODO(henrika): investigate if there is a need for a "deeper" getter.
  return agc_is_enabled_;
}

int32_t WebRtcAudioDeviceImpl::SetWaveOutVolume(uint16_t volume_left,
                                                uint16_t volume_right) {
  NOTIMPLEMENTED();
  return -1;
}
int32_t WebRtcAudioDeviceImpl::WaveOutVolume(
    uint16_t* volume_left,
    uint16_t* volume_right) const {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SpeakerIsAvailable(bool* available) {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SpeakerIsAvailable() "
           << "NOT IMPLEMENTED";
  *available = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::InitSpeaker() {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::InitSpeaker() "
           << "NOT IMPLEMENTED";
  return 0;
}

bool WebRtcAudioDeviceImpl::SpeakerIsInitialized() const {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SpeakerIsInitialized() "
           << "NOT IMPLEMENTED";
  return true;
}

int32_t WebRtcAudioDeviceImpl::MicrophoneIsAvailable(bool* available) {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::MicrophoneIsAvailable() "
           << "NOT IMPLEMENTED";
  *available = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::InitMicrophone() {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::InitMicrophone() "
           << "NOT IMPLEMENTED";
  return 0;
}

bool WebRtcAudioDeviceImpl::MicrophoneIsInitialized() const {
  NOTIMPLEMENTED();
  return true;
}

int32_t WebRtcAudioDeviceImpl::SpeakerVolumeIsAvailable(
    bool* available) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SetSpeakerVolume(uint32_t volume) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SpeakerVolume(uint32_t* volume) const {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::MaxSpeakerVolume(uint32_t* max_volume) const {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::MinSpeakerVolume(uint32_t* min_volume) const {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SpeakerVolumeStepSize(
    uint16_t* step_size) const {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::MicrophoneVolumeIsAvailable(bool* available) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SetMicrophoneVolume(uint32_t volume) {
  DCHECK(initialized_);
  DVLOG(1) << "SetMicrophoneVolume(" << volume << ")";
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

int32_t WebRtcAudioDeviceImpl::MicrophoneVolume(uint32_t* volume) const {
  // The microphone level is fed to this class using the Capture() callback
  // and this external API should not be used. Additional IPC messages are
  // required if support for this API is ever needed.
  NOTREACHED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::MaxMicrophoneVolume(uint32_t* max_volume) const {
  *max_volume = kMaxVolumeLevel;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::MinMicrophoneVolume(uint32_t* min_volume) const {
  *min_volume = 0;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::MicrophoneVolumeStepSize(
    uint16_t* step_size) const {
  NOTREACHED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SpeakerMuteIsAvailable(bool* available) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SetSpeakerMute(bool enable) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SpeakerMute(bool* enabled) const {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::MicrophoneMuteIsAvailable(
    bool* available) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SetMicrophoneMute(bool enable) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::MicrophoneMute(bool* enabled) const {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::MicrophoneBoostIsAvailable(bool* available) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SetMicrophoneBoost(bool enable) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::MicrophoneBoost(bool* enabled) const {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::StereoPlayoutIsAvailable(bool* available) const {
  DCHECK(initialized_) << "Init() must be called first.";

  *available = (output_channels() == 2);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetStereoPlayout(bool enable) {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetStereoPlayout() "
           << "NOT IMPLEMENTED";
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StereoPlayout(bool* enabled) const {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::StereoPlayout() "
           << "NOT IMPLEMENTED";
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StereoRecordingIsAvailable(
    bool* available) const {
  DCHECK(initialized_) << "Init() must be called first.";
  if (!capturer_)
    return -1;

  *available = (input_channels() == 2);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetStereoRecording(bool enable) {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetStereoRecording() "
           << "NOT IMPLEMENTED";
  return -1;
}

int32_t WebRtcAudioDeviceImpl::StereoRecording(bool* enabled) const {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::StereoRecording() "
           << "NOT IMPLEMENTED";
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SetRecordingChannel(const ChannelType channel) {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetRecordingChannel() "
           << "NOT IMPLEMENTED";
  return -1;
}

int32_t WebRtcAudioDeviceImpl::RecordingChannel(ChannelType* channel) const {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::RecordingChannel() "
           << "NOT IMPLEMENTED";
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SetPlayoutBuffer(const BufferType type,
                                                uint16_t size_ms) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::PlayoutBuffer(BufferType* type,
                                             uint16_t* size_ms) const {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::PlayoutDelay(uint16_t* delay_ms) const {
  // Report the cached output delay value.
  base::AutoLock auto_lock(lock_);
  *delay_ms = static_cast<uint16_t>(output_delay_ms_);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::RecordingDelay(uint16_t* delay_ms) const {
  // Report the cached output delay value.
  base::AutoLock auto_lock(lock_);
  *delay_ms = static_cast<uint16_t>(input_delay_ms_);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::CPULoad(uint16_t* load) const {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::StartRawOutputFileRecording(
    const char pcm_file_name_utf8[webrtc::kAdmMaxFileNameSize]) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::StopRawOutputFileRecording() {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::StartRawInputFileRecording(
    const char pcm_file_name_utf8[webrtc::kAdmMaxFileNameSize]) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::StopRawInputFileRecording() {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SetRecordingSampleRate(
    const uint32_t samples_per_sec) {
  // Sample rate should only be set at construction.
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::RecordingSampleRate(
    uint32_t* samples_per_sec) const {
  // Returns the sample rate set at construction.
  *samples_per_sec = static_cast<uint32_t>(input_sample_rate());
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetPlayoutSampleRate(
    const uint32_t samples_per_sec) {
  // Sample rate should only be set at construction.
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::PlayoutSampleRate(
    uint32_t* samples_per_sec) const {
  // Returns the sample rate set at construction.
  *samples_per_sec = static_cast<uint32_t>(output_sample_rate());
  return 0;
}

int32_t WebRtcAudioDeviceImpl::ResetAudioDevice() {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SetLoudspeakerStatus(bool enable) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::GetLoudspeakerStatus(bool* enabled) const {
  NOTIMPLEMENTED();
  return -1;
}

}  // namespace content
