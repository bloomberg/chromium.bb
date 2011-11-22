// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_device_impl.h"

#include "base/bind.h"
#include "base/string_util.h"
#include "base/win/windows_version.h"
#include "content/renderer/media/audio_hardware.h"
#include "content/renderer/render_thread_impl.h"
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
    DVLOG(1) << "WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()";
    DCHECK(RenderThreadImpl::current()) <<
        "WebRtcAudioDeviceImpl must be constructed on the render thread";
}

WebRtcAudioDeviceImpl::~WebRtcAudioDeviceImpl() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::~WebRtcAudioDeviceImpl()";
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

  int samples_per_sec = static_cast<int>(input_sample_rate_);
  if (samples_per_sec == 44100) {
    // Even if the hardware runs at 44.1kHz, we use 44.0 internally.
    samples_per_sec = 44000;
  }
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

void WebRtcAudioDeviceImpl::OnDeviceStarted(const std::string& device_id) {
  VLOG(1) << "OnDeviceStarted (device_id=" << device_id << ")";
  // Empty string is an invalid device id. Do nothing if a valid device has
  // been started. Otherwise update the |recording_| state to false.
  if (!device_id.empty())
    return;

  base::AutoLock auto_lock(lock_);
  if (recording_)
    recording_ = false;
}

void WebRtcAudioDeviceImpl::OnDeviceStopped() {
  DVLOG(1) << "OnDeviceStopped";
  base::AutoLock auto_lock(lock_);
  if (recording_)
    recording_ = false;
}

int32_t WebRtcAudioDeviceImpl::Version(char* version,
                                       uint32_t& remaining_buffer_in_bytes,
                                       uint32_t& position) const {
  DVLOG(1) << "Version()";
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
  DVLOG(1) << "version: " << version;
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
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::RegisterEventObserver() "
           << "NOT IMPLEMENTED";
  return -1;
}

int32_t WebRtcAudioDeviceImpl::RegisterAudioCallback(
    webrtc::AudioTransport* audio_callback) {
  DVLOG(1) << "RegisterAudioCallback()";
  if (playing_ || recording_)  {
    LOG(ERROR) << "Unable to (de)register transport during active media";
    return -1;
  }
  audio_transport_callback_ = audio_callback;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::Init() {
  DVLOG(1) << "Init()";

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
  if (initialized_)
    return 0;

  DCHECK(!audio_input_device_);
  DCHECK(!audio_output_device_);
  DCHECK(!input_buffer_.get());
  DCHECK(!output_buffer_.get());

  // Ask the browser for the default audio output hardware sample-rate.
  // This request is based on a synchronous IPC message.
  int output_sample_rate =
      static_cast<int>(audio_hardware::GetOutputSampleRate());
  DVLOG(1) << "Audio output hardware sample rate: " << output_sample_rate;

  // Ask the browser for the default audio input hardware sample-rate.
  // This request is based on a synchronous IPC message.
  int input_sample_rate =
      static_cast<int>(audio_hardware::GetInputSampleRate());
  DVLOG(1) << "Audio input hardware sample rate: " << input_sample_rate;

  int input_channels = 0;
  int output_channels = 0;

  size_t input_buffer_size = 0;
  size_t output_buffer_size = 0;

// Windows
#if defined(OS_WIN)
  if (input_sample_rate != 48000 && input_sample_rate != 44100) {
    DLOG(ERROR) << "Only 48 and 44.1kHz input rates are supported on Windows.";
    return -1;
  }
  if (output_sample_rate != 48000 && output_sample_rate != 44100) {
    DLOG(ERROR) << "Only 48 and 44.1kHz output rates are supported on Windows.";
    return -1;
  }

  // Use stereo recording on Windows since low-latency Core Audio (WASAPI)
  // does not support mono.
  input_channels = 2;

  // Use stereo rendering on Windows to make input and output sides
  // symmetric. WASAPI supports both stereo and mono.
  output_channels = 2;

  // Capture side: AUDIO_PCM_LOW_LATENCY is based on the Core Audio (WASAPI)
  // API which was introduced in Windows Vista. For lower Windows versions,
  // a callback-driven Wave implementation is used instead. An input buffer
  // size of 10ms works well for both these implementations.

  // Use different buffer sizes depending on the current hardware sample rate.
  if (input_sample_rate == 48000) {
    input_buffer_size = 480;
  } else {
    // We do run at 44.1kHz at the actual audio layer, but ask for frames
    // at 44.0kHz to ensure that we can feed them to the webrtc::VoiceEngine.
    input_buffer_size = 440;
  }

  // Render side: AUDIO_PCM_LOW_LATENCY is based on the Core Audio (WASAPI)
  // API which was introduced in Windows Vista. For lower Windows versions,
  // a callback-driven Wave implementation is used instead. An output buffer
  // size of 10ms works well for WASAPI but 30ms is needed for Wave.

  // Use different buffer sizes depending on the current hardware sample rate.
  if (output_sample_rate == 48000) {
    output_buffer_size = 480;
  } else {
    // We do run at 44.1kHz at the actual audio layer, but ask for frames
    // at 44.0kHz to ensure that we can feed them to the webrtc::VoiceEngine.
    // TODO(henrika): figure out why we seem to need 20ms here for glitch-
    // free audio.
    output_buffer_size = 2 * 440;
  }

  // Windows XP and lower can't cope with 10 ms output buffer size.
  // It must be extended to 30 ms (60 ms will be used internally by WaveOut).
  if (base::win::GetVersion() <= base::win::VERSION_XP) {
    output_buffer_size = 3 * output_buffer_size;
    DLOG(WARNING) << "Extending the output buffer size by a factor of three "
                  << "since Windows XP has been detected.";
  }

// Mac OS X
#elif defined(OS_MACOSX)
  if (input_sample_rate != 48000 && input_sample_rate != 44100) {
    DLOG(ERROR) << "Only 48 and 44.1kHz input rates are supported on Mac OSX.";
    return -1;
  }
  if (output_sample_rate != 48000 && output_sample_rate != 44100) {
    DLOG(ERROR) << "Only 48 and 44.1kHz output rates are supported on Mac OSX.";
    return -1;
  }

  input_channels = 1;
  output_channels = 1;

  // Capture side: AUDIO_PCM_LOW_LATENCY on Mac OS X is based on a callback-
  // driven Core Audio implementation. Tests have shown that 10ms is a suitable
  // frame size to use, both for 48kHz and 44.1kHz.

  // Use different buffer sizes depending on the current hardware sample rate.
  if (input_sample_rate == 48000) {
    input_buffer_size = 480;
  } else {
    // We do run at 44.1kHz at the actual audio layer, but ask for frames
    // at 44.0kHz to ensure that we can feed them to the webrtc::VoiceEngine.
    input_buffer_size = 440;
  }

  // Render side: AUDIO_PCM_LOW_LATENCY on Mac OS X is based on a callback-
  // driven Core Audio implementation. Tests have shown that 10ms is a suitable
  // frame size to use, both for 48kHz and 44.1kHz.

  // Use different buffer sizes depending on the current hardware sample rate.
  if (output_sample_rate == 48000) {
    output_buffer_size = 480;
  } else {
    // We do run at 44.1kHz at the actual audio layer, but ask for frames
    // at 44.0kHz to ensure that we can feed them to the webrtc::VoiceEngine.
    output_buffer_size = 440;
  }
// Linux
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
  if (output_sample_rate != 48000) {
    DLOG(ERROR) << "Only 48kHz sample rate is supported on Linux.";
    return -1;
  }
  input_channels = 2;
  output_channels = 1;

  // Based on tests using the current ALSA implementation in Chrome, we have
  // found that the best combination is 20ms on the input side and 10ms on the
  // output side.
  // TODO(henrika): It might be possible to reduce the input buffer
  // size and reduce the delay even more.
  input_buffer_size = 2 * 480;
  output_buffer_size = 480;
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
  input_sample_rate_ = input_sample_rate;

  // Create and configure the audio capturing client.
  audio_input_device_ = new AudioInputDevice(
      input_buffer_size, input_channels, input_sample_rate, this, this);

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

  DVLOG(1) << "Capture parameters (size/channels/rate): ("
           << input_buffer_size_ << "/" << input_channels_ << "/"
           << input_sample_rate_ << ")";
  DVLOG(1) << "Render parameters (size/channels/rate): ("
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
  DVLOG(1) << "Terminate()";

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
  *available = (audio_output_device_ != NULL);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::InitPlayout() {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::InitPlayout() "
           << "NOT IMPLEMENTED";
  return 0;
}

bool WebRtcAudioDeviceImpl::PlayoutIsInitialized() const {
  DVLOG(1) << "PlayoutIsInitialized()";
  return (audio_output_device_ != NULL);
}

int32_t WebRtcAudioDeviceImpl::RecordingIsAvailable(bool* available) {
  DVLOG(1) << "RecordingIsAvailable()";
  *available = (audio_input_device_ != NULL);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::InitRecording() {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::InitRecording() "
           << "NOT IMPLEMENTED";
  return 0;
}

bool WebRtcAudioDeviceImpl::RecordingIsInitialized() const {
  DVLOG(1) << "RecordingIsInitialized()";
  return (audio_input_device_ != NULL);
}

int32_t WebRtcAudioDeviceImpl::StartPlayout() {
  DVLOG(1) << "StartPlayout()";
  if (!audio_transport_callback_) {
    LOG(ERROR) << "Audio transport is missing";
    return -1;
  }
  if (playing_) {
    // webrtc::VoiceEngine assumes that it is OK to call Start() twice and
    // that the call is ignored the second time.
    return 0;
  }
  audio_output_device_->Start();
  playing_ = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopPlayout() {
  DVLOG(1) << "StopPlayout()";
  DCHECK(audio_output_device_);
  if (!playing_) {
    // webrtc::VoiceEngine assumes that it is OK to call Stop() just in case.
    return 0;
  }
  playing_ = !audio_output_device_->Stop();
  return (!playing_ ? 0 : -1);
}

bool WebRtcAudioDeviceImpl::Playing() const {
  return playing_;
}

int32_t WebRtcAudioDeviceImpl::StartRecording() {
  DVLOG(1) << "StartRecording()";
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
    return 0;
  }

  // Specify the session_id which is mapped to a certain device.
  audio_input_device_->SetDevice(session_id_);
  audio_input_device_->Start();
  recording_ = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopRecording() {
  DVLOG(1) << "StopRecording()";
  DCHECK(audio_input_device_);

  base::AutoLock auto_lock(lock_);
  if (!recording_) {
    // webrtc::VoiceEngine assumes that it is OK to call Stop() just in case.
    return 0;
  }
  recording_ = !audio_input_device_->Stop();
  return (!recording_ ? 0 : -1);
}

bool WebRtcAudioDeviceImpl::Recording() const {
  return recording_;
}

int32_t WebRtcAudioDeviceImpl::SetAGC(bool enable) {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::SetAGC() " << "NOT IMPLEMENTED";
  return -1;
}

bool WebRtcAudioDeviceImpl::AGC() const {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::AGC() " << "NOT IMPLEMENTED";
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
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::MaxMicrophoneVolume() "
           << "NOT IMPLEMENTED";
  return -1;
}

int32_t WebRtcAudioDeviceImpl::MinMicrophoneVolume(
    uint32_t* min_volume) const {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::MinMicrophoneVolume() "
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
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::StereoPlayoutIsAvailable() "
           << "NOT IMPLEMENTED";
  *available = false;
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
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::StereoRecordingIsAvailable() "
           << "NOT IMPLEMENTED";
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
