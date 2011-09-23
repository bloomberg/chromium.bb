// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_device_impl.h"

#include "base/string_util.h"
#include "content/renderer/render_thread.h"
#include "media/audio/audio_util.h"

static const int64 kMillisecondsBetweenProcessCalls = 5000;
static const char kVersion[] = "WebRTC AudioDevice 1.0.0.Chrome";

WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()
    : ref_count_(0),
      render_loop_(base::MessageLoopProxy::current()),
      audio_transport_callback_(NULL),
      input_buffer_size_(0),
      output_buffer_size_(0),
      input_channels_(0),
      output_channels_(0),
      input_sample_rate_(0),
      output_sample_rate_(0),
      input_delay_ms_(0),
      output_delay_ms_(0),
      last_error_(AudioDeviceModule::kAdmErrNone),
      last_process_time_(base::TimeTicks::Now()),
      session_id_(0),
      initialized_(false),
      playing_(false),
      recording_(false) {
    VLOG(1) << "WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()";
    DCHECK(RenderThread::current()) <<
        "WebRtcAudioDeviceImpl must be constructed on the render thread";
}

WebRtcAudioDeviceImpl::~WebRtcAudioDeviceImpl() {
  VLOG(1) << "WebRtcAudioDeviceImpl::~WebRtcAudioDeviceImpl()";
  if (playing_)
    StopPlayout();
  if (recording_)
    StopRecording();
  if (initialized_)
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

void WebRtcAudioDeviceImpl::Render(
    const std::vector<float*>& audio_data,
    size_t number_of_frames,
    size_t audio_delay_milliseconds) {
  DCHECK_LE(number_of_frames, output_buffer_size_);

  // Store the reported audio delay locally.
  output_delay_ms_ = audio_delay_milliseconds;

  const int channels = audio_data.size();
  DCHECK_LE(channels, output_channels_);

  int samples_per_sec = static_cast<int>(output_sample_rate_);
  if (samples_per_sec == 44100) {
    // Even if the hardware runs at 44.1kHz, we use 44.0 internally.
    // Can only happen on Mac OS X currently since Windows and Mac
    // both uses 48kHz.
    samples_per_sec = 44000;
  }
  uint32_t samples_per_10_msec = (samples_per_sec / 100);
  const int bytes_per_10_msec =
      channels * samples_per_10_msec * bytes_per_sample_;

  uint32_t num_audio_samples = 0;
  size_t accumulated_audio_samples = 0;

  char* audio_byte_buffer = reinterpret_cast<char*>(output_buffer_.get());

  // Get audio samples in blocks of 10 milliseconds from the registered
  // webrtc::AudioTransport source. Keep reading until our internal buffer
  // is full.
  while (accumulated_audio_samples < number_of_frames) {
    // Get 10ms and append output to temporary byte buffer.
    audio_transport_callback_->NeedMorePlayData(samples_per_10_msec,
                                                bytes_per_sample_,
                                                channels,
                                                samples_per_sec,
                                                audio_byte_buffer,
                                                num_audio_samples);
    accumulated_audio_samples += num_audio_samples;
    audio_byte_buffer += bytes_per_10_msec;
  }

  // Deinterleave each channel and convert to 32-bit floating-point
  // with nominal range -1.0 -> +1.0 to match the callback format.
  for (int channel_index = 0; channel_index < channels; ++channel_index) {
    media::DeinterleaveAudioChannel(
        output_buffer_.get(),
        audio_data[channel_index],
        channels,
        channel_index,
        bytes_per_sample_,
        number_of_frames);
  }
}

void WebRtcAudioDeviceImpl::Capture(
    const std::vector<float*>& audio_data,
    size_t number_of_frames,
    size_t audio_delay_milliseconds) {
  DCHECK_LE(number_of_frames, input_buffer_size_);

  // Store the reported audio delay locally.
  input_delay_ms_ = audio_delay_milliseconds;

  const int channels = audio_data.size();
  DCHECK_LE(channels, input_channels_);
  uint32_t new_mic_level = 0;

  // Interleave, scale, and clip input to int16 and store result in
  // a local byte buffer.
  media::InterleaveFloatToInt16(audio_data,
                                input_buffer_.get(),
                                number_of_frames);

  const int samples_per_sec = static_cast<int>(input_sample_rate_);
  const int samples_per_10_msec = (samples_per_sec / 100);
  const int bytes_per_10_msec =
      channels * samples_per_10_msec * bytes_per_sample_;
  size_t accumulated_audio_samples = 0;

  char* audio_byte_buffer = reinterpret_cast<char*>(input_buffer_.get());

  // Write audio samples in blocks of 10 milliseconds to the registered
  // webrtc::AudioTransport sink. Keep writing until our internal byte
  // buffer is empty.
  while (accumulated_audio_samples < number_of_frames) {
    // Deliver 10ms of recorded PCM audio.
    // TODO(henrika): add support for analog AGC?
    audio_transport_callback_->RecordedDataIsAvailable(
        audio_byte_buffer,
        samples_per_10_msec,
        bytes_per_sample_,
        channels,
        samples_per_sec,
        input_delay_ms_ + output_delay_ms_,
        0,  // clock_drift
        0,  // current_mic_level
        new_mic_level);  // not used
    accumulated_audio_samples += samples_per_10_msec;
    audio_byte_buffer += bytes_per_10_msec;
  }
}

void WebRtcAudioDeviceImpl::OnDeviceStarted(int device_index) {
  VLOG(1) << "OnDeviceStarted (device_index=" << device_index << ")";
  // -1 is an invalid device index. Do nothing if a valid device has
  // been started. Otherwise update the |recording_| state to false.
  if (device_index != -1)
    return;

  base::AutoLock auto_lock(lock_);
  if (recording_)
    recording_ = false;
}

void WebRtcAudioDeviceImpl::OnDeviceStopped() {
  VLOG(1) << "OnDeviceStopped";
  base::AutoLock auto_lock(lock_);
  if (recording_)
    recording_ = false;
}

int32_t WebRtcAudioDeviceImpl::Version(char* version,
                                       uint32_t& remaining_buffer_in_bytes,
                                       uint32_t& position) const {
  VLOG(1) << "Version()";
  DCHECK(version);
  if (version == NULL)
    return -1;
  size_t arr_size = arraysize(kVersion);
  if (remaining_buffer_in_bytes < arr_size) {
    DLOG(WARNING) << "version string requires " << arr_size << " bytes";
    return -1;
  }
  base::strlcpy(&version[position], kVersion, arr_size - 1);
  remaining_buffer_in_bytes -= arr_size;
  position += arr_size;
  VLOG(1) << "version: " << version;
  return 0;
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
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::RegisterEventObserver() "
          << "NOT IMPLEMENTED";
  return -1;
}

int32_t WebRtcAudioDeviceImpl::RegisterAudioCallback(
    webrtc::AudioTransport* audio_callback) {
  VLOG(1) << "RegisterAudioCallback()";
  if (playing_ || recording_)  {
    LOG(ERROR) << "Unable to (de)register transport during active media";
    return -1;
  }
  audio_transport_callback_ = audio_callback;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::Init() {
  VLOG(1) << "Init()";

  if (!render_loop_->BelongsToCurrentThread()) {
    int32_t error = 0;
    base::WaitableEvent event(false, false);
    // Ensure that we call Init() from the main render thread since
    // the audio clients can only be created on this thread.
    render_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &WebRtcAudioDeviceImpl::InitOnRenderThread,
                          &error, &event));
    event.Wait();
    return error;
  }

  // Calling Init() multiple times in a row is OK.
  if (initialized_)
    return 0;

  DCHECK(!audio_input_device_);
  DCHECK(!audio_output_device_);
  DCHECK(!input_buffer_.get());
  DCHECK(!output_buffer_.get());

  // TODO(henrika): add AudioInputDevice::GetAudioHardwareSampleRate().
  // Assume that input and output sample rates are identical for now.

  // Ask the browser for the default audio output hardware sample-rate.
  // This request is based on a synchronous IPC message.
  int output_sample_rate =
      static_cast<int>(AudioDevice::GetAudioHardwareSampleRate());
  VLOG(1) << "Audio hardware sample rate: " << output_sample_rate;

  int input_channels = 0;
  int output_channels = 0;

  size_t input_buffer_size = 0;
  size_t output_buffer_size = 0;

  // For real-time audio (in combination with the webrtc::VoiceEngine) it
  // is convenient to use audio buffers of size N*10ms.
#if defined(OS_WIN)
  if (output_sample_rate != 48000) {
    DLOG(ERROR) << "Only 48kHz sample rate is supported on Windows.";
    return -1;
  }
  input_channels = 1;
  output_channels = 1;
  // Capture side: AUDIO_PCM_LINEAR on Windows is based on a callback-
  // driven Wave implementation where 3 buffers are used for recording audio.
  // Each buffer is of the size that we specify here and using more than one
  // does not increase the delay but only adds robustness against dropping
  // audio. It might also affect the initial start-up time before callbacks
  // start to pump. Real-time tests have shown that a buffer size of 10ms
  // works fine on the capture side.
  input_buffer_size = 480;
  // Rendering side: AUDIO_PCM_LOW_LATENCY on Windows is based on a callback-
  // driven Wave implementation where 2 buffers are fed to the audio driver
  // before actual rendering starts. Initial real-time tests have shown that
  // 20ms buffer size (corresponds to ~40ms total delay) is not enough but
  // can lead to buffer underruns. The next even multiple of 10ms is 30ms
  // (<=> ~60ms total delay) and it works fine also under high load.
  output_buffer_size = 3 * 480;
#elif defined(OS_MACOSX)
  if (output_sample_rate != 48000 && output_sample_rate != 44100) {
    DLOG(ERROR) << "Only 48 and 44.1kHz sample rates are supported on Mac OSX.";
    return -1;
  }
  input_channels = 1;
  output_channels = 1;
  // Rendering side: AUDIO_PCM_LOW_LATENCY on Mac OS X is based on a callback-
  // driven Core Audio implementation. Tests have shown that 10ms is a suitable
  // frame size to use, both for 48kHz and 44.1kHz.
  // Capturing side: AUDIO_PCM_LINEAR on Mac OS X uses the Audio Queue Services
  // API which is not well suited for real-time applications since the delay
  // is very high. We set buffer sizes to 10ms for the input side here as well
  // but none of them will work.
  // TODO(henrika): add support for AUDIO_PCM_LOW_LATENCY on the capture side
  // based on the Mac OS X Core Audio API.

  // Use different buffer sizes depending on the current hardware sample rate.
  if (output_sample_rate == 48000) {
    input_buffer_size = 480;
    output_buffer_size = 480;
  } else {
    // We do run at 44.1kHz at the actual audio layer, but ask for frames
    // at 44.0kHz to ensure that we can feed them to the webrtc::VoiceEngine.
    input_buffer_size = 440;
    output_buffer_size = 440;
  }
#elif defined(OS_LINUX)
  if (output_sample_rate != 48000) {
    DLOG(ERROR) << "Only 48kHz sample rate is supported on Linux.";
    return -1;
  }
  input_channels = 1;
  output_channels = 1;
  // Based on tests using the current ALSA implementation in Chrome, we have
  // found that the best combination is 20ms on the input side and 30ms on the
  // output side.
  // TODO(henrika): It might be possible to reduce the input and output buffer
  // size and reduce the delay even more.
  input_buffer_size = 2 * 480;
  output_buffer_size = 3 * 480;
#else
  DLOG(ERROR) << "Unsupported platform";
  return -1;
#endif

  // Store utilized parameters to ensure that we can check them
  // after a successful initialization.
  output_buffer_size_ = output_buffer_size;
  output_channels_ = output_channels;
  output_sample_rate_ = static_cast<double>(output_sample_rate);

  input_buffer_size_ = input_buffer_size;
  input_channels_ = input_channels;
  // TODO(henrika): we use same rate as on output for now.
  input_sample_rate_ = output_sample_rate_;

  // Create and configure the audio capturing client.
  audio_input_device_ = new AudioInputDevice(
      input_buffer_size, input_channels, output_sample_rate, this, this);
#if defined(OS_MACOSX)
  // We create the input device for Mac as well but the performance
  // will be very bad.
  DLOG(WARNING) << "Real-time recording is not yet supported on Mac OS X";
#endif

  // Create and configure the audio rendering client.
  audio_output_device_ = new AudioDevice(
      output_buffer_size, output_channels, output_sample_rate, this);

  DCHECK(audio_input_device_);
  DCHECK(audio_output_device_);

  // Allocate local audio buffers based on the parameters above.
  // It is assumed that each audio sample contains 16 bits and each
  // audio frame contains one or two audio samples depending on the
  // number of channels.
  input_buffer_.reset(new int16[input_buffer_size * input_channels]);
  output_buffer_.reset(new int16[output_buffer_size * output_channels]);

  DCHECK(input_buffer_.get());
  DCHECK(output_buffer_.get());

  bytes_per_sample_ = sizeof(*input_buffer_.get());

  initialized_ = true;

  VLOG(1) << "Capture parameters (size/channels/rate): ("
          << input_buffer_size_ << "/" << input_channels_ << "/"
          << input_sample_rate_ << ")";
  VLOG(1) << "Render parameters (size/channels/rate): ("
          << output_buffer_size_ << "/" << output_channels_ << "/"
          << output_sample_rate_ << ")";
  return 0;
}

void WebRtcAudioDeviceImpl::InitOnRenderThread(int32_t* error,
                                               base::WaitableEvent* event) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  *error = Init();
  event->Signal();
}

int32_t WebRtcAudioDeviceImpl::Terminate() {
  VLOG(1) << "Terminate()";

  // Calling Terminate() multiple times in a row is OK.
  if (!initialized_)
    return 0;

  DCHECK(audio_input_device_);
  DCHECK(audio_output_device_);
  DCHECK(input_buffer_.get());
  DCHECK(output_buffer_.get());

  // Release all resources allocated in Init().
  audio_input_device_ = NULL;
  audio_output_device_ = NULL;
  input_buffer_.reset();
  output_buffer_.reset();

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
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetPlayoutDevice() "
          << "NOT IMPLEMENTED";
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetPlayoutDevice(WindowsDeviceType device) {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetPlayoutDevice() "
          << "NOT IMPLEMENTED";
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetRecordingDevice(uint16_t index) {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetRecordingDevice() "
          << "NOT IMPLEMENTED";
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetRecordingDevice(WindowsDeviceType device) {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetRecordingDevice() "
          << "NOT IMPLEMENTED";
  return 0;
}

int32_t WebRtcAudioDeviceImpl::PlayoutIsAvailable(bool* available) {
  VLOG(1) << "PlayoutIsAvailable()";
  *available = (audio_output_device_ != NULL);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::InitPlayout() {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::InitPlayout() "
          << "NOT IMPLEMENTED";
  return 0;
}

bool WebRtcAudioDeviceImpl::PlayoutIsInitialized() const {
  VLOG(1) << "PlayoutIsInitialized()";
  return (audio_output_device_ != NULL);
}

int32_t WebRtcAudioDeviceImpl::RecordingIsAvailable(bool* available) {
  VLOG(1) << "RecordingIsAvailable()";
  *available = (audio_input_device_ != NULL);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::InitRecording() {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::InitRecording() "
          << "NOT IMPLEMENTED";
  return 0;
}

bool WebRtcAudioDeviceImpl::RecordingIsInitialized() const {
  VLOG(1) << "RecordingIsInitialized()";
  return (audio_input_device_ != NULL);
}

int32_t WebRtcAudioDeviceImpl::StartPlayout() {
  VLOG(1) << "StartPlayout()";
  if (!audio_transport_callback_) {
    LOG(ERROR) << "Audio transport is missing";
    return -1;
  }
  if (playing_) {
    // webrtc::VoiceEngine assumes that it is OK to call Start() twice and
    // that the call is ignored the second time.
    LOG(WARNING) << "Playout is already active";
    return 0;
  }
  audio_output_device_->Start();
  playing_ = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopPlayout() {
  VLOG(1) << "StopPlayout()";
  DCHECK(audio_output_device_);
  if (!playing_) {
    // webrtc::VoiceEngine assumes that it is OK to call Stop() just in case.
    LOG(WARNING) << "Playout was already stopped";
    return 0;
  }
  playing_ = !audio_output_device_->Stop();
  return (!playing_ ? 0 : -1);
}

bool WebRtcAudioDeviceImpl::Playing() const {
  return playing_;
}

int32_t WebRtcAudioDeviceImpl::StartRecording() {
  VLOG(1) << "StartRecording()";
#if defined(OS_MACOSX)
  DLOG(WARNING) << "Real-time recording is not yet fully supported on Mac OS X";
#endif
  LOG_IF(ERROR, !audio_transport_callback_) << "Audio transport is missing";
  if (!audio_transport_callback_) {
    LOG(ERROR) << "Audio transport is missing";
    return -1;
  }

  if (session_id_ <= 0) {
    LOG(WARNING) << session_id_ << " is an invalid session id.";
    // TODO(xians): enable the return -1 when MediaStreamManager can handle
    // AudioInputDeviceManager.
//    return -1;
  }

  base::AutoLock auto_lock(lock_);
  if (recording_) {
    // webrtc::VoiceEngine assumes that it is OK to call Start() twice and
    // that the call is ignored the second time.
    LOG(WARNING) << "Recording is already active";
    return 0;
  }

  // Specify the session_id which is mapped to a certain device.
  audio_input_device_->SetDevice(session_id_);
  audio_input_device_->Start();
  recording_ = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopRecording() {
  VLOG(1) << "StopRecording()";
  DCHECK(audio_input_device_);

  base::AutoLock auto_lock(lock_);
  if (!recording_) {
    // webrtc::VoiceEngine assumes that it is OK to call Stop() just in case.
    LOG(WARNING) << "Recording was already stopped";
    return 0;
  }
  recording_ = !audio_input_device_->Stop();
  return (!recording_ ? 0 : -1);
}

bool WebRtcAudioDeviceImpl::Recording() const {
  return recording_;
}

int32_t WebRtcAudioDeviceImpl::SetAGC(bool enable) {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetAGC() "
          << "NOT IMPLEMENTED";
  return -1;
}

bool WebRtcAudioDeviceImpl::AGC() const {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::AGC() "
          << "NOT IMPLEMENTED";
  return false;
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
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SpeakerIsAvailable() "
          << "NOT IMPLEMENTED";
  *available = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::InitSpeaker() {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::InitSpeaker() "
          << "NOT IMPLEMENTED";
  return 0;
}

bool WebRtcAudioDeviceImpl::SpeakerIsInitialized() const {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SpeakerIsInitialized() "
          << "NOT IMPLEMENTED";
  return true;
}

int32_t WebRtcAudioDeviceImpl::MicrophoneIsAvailable(bool* available) {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::MicrophoneIsAvailable() "
          << "NOT IMPLEMENTED";
  *available = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::InitMicrophone() {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::InitMicrophone() "
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

int32_t WebRtcAudioDeviceImpl::MinSpeakerVolume(
    uint32_t* min_volume) const {
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
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::MicrophoneVolume(uint32_t* volume) const {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::MaxMicrophoneVolume(
    uint32_t* max_volume) const {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::MaxMicrophoneVolume() "
          << "NOT IMPLEMENTED";
  return -1;
}

int32_t WebRtcAudioDeviceImpl::MinMicrophoneVolume(
    uint32_t* min_volume) const {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::MinMicrophoneVolume() "
          << "NOT IMPLEMENTED";
  return -1;
}

int32_t WebRtcAudioDeviceImpl::MicrophoneVolumeStepSize(
    uint16_t* step_size) const {
  NOTIMPLEMENTED();
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
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::StereoPlayoutIsAvailable() "
          << "NOT IMPLEMENTED";
  *available = false;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetStereoPlayout(bool enable) {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetStereoPlayout() "
          << "NOT IMPLEMENTED";
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StereoPlayout(bool* enabled) const {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::StereoPlayout() "
          << "NOT IMPLEMENTED";
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StereoRecordingIsAvailable(
    bool* available) const {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::StereoRecordingIsAvailable() "
          << "NOT IMPLEMENTED";
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetStereoRecording(bool enable) {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetStereoRecording() "
          << "NOT IMPLEMENTED";
  return -1;
}

int32_t WebRtcAudioDeviceImpl::StereoRecording(bool* enabled) const {
  VLOG(2) << "WARNING: WebRtcAudioDeviceImpl::StereoRecording() "
          << "NOT IMPLEMENTED";
  return -1;
}

int32_t WebRtcAudioDeviceImpl::SetRecordingChannel(const ChannelType channel) {
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::RecordingChannel(ChannelType* channel) const {
  NOTIMPLEMENTED();
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
  *delay_ms = static_cast<uint16_t>(output_delay_ms_);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::RecordingDelay(uint16_t* delay_ms) const {
  // Report the cached output delay value.
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
  *samples_per_sec = static_cast<uint32_t>(input_sample_rate_);
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
  *samples_per_sec = static_cast<uint32_t>(output_sample_rate_);
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

void WebRtcAudioDeviceImpl::SetSessionId(int session_id) {
  session_id_ = session_id;
}
