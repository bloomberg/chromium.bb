// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_device_impl.h"

#include "base/string_util.h"
#include "media/audio/audio_util.h"

// TODO(henrika): come up with suitable value(s) for all platforms.
// Max supported size for input and output buffers.
// Unit is in #(audio frames), hence 1440 <=> 30ms @ 48kHz.
static const size_t kMaxBufferSize = 1440;
static const int kMaxChannels = 2;
static const int64 kMillisecondsBetweenProcessCalls = 5000;
static const char kVersion[] = "WebRTC AudioDevice 1.0.0.Chrome";

WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl(
  size_t input_buffer_size, size_t output_buffer_size,
  int input_channels, int output_channels,
  double input_sample_rate, double output_sample_rate)
    : audio_transport_callback_(NULL),
      last_error_(AudioDeviceModule::kAdmErrNone),
      input_buffer_size_(input_buffer_size),
      output_buffer_size_(output_buffer_size),
      input_channels_(input_channels),
      output_channels_(output_channels),
      input_sample_rate_(input_sample_rate),
      output_sample_rate_(output_sample_rate),
      initialized_(false),
      playing_(false),
      recording_(false),
      input_delay_ms_(0),
      output_delay_ms_(0),
      last_process_time_(base::TimeTicks::Now()) {
    VLOG(1) << "WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()";

    // Create an AudioInputDevice client if the requested buffer size
    // is an even multiple of 10 milliseconds.
    if (BufferSizeIsValid(input_buffer_size, input_sample_rate)) {
      audio_input_device_ = new AudioInputDevice(
          input_buffer_size,
          input_channels,
          input_sample_rate,
          this);
    }

    // Create an AudioDevice client if the requested buffer size
    // is an even multiple of 10 milliseconds.
    if (BufferSizeIsValid(output_buffer_size, output_sample_rate)) {
      audio_output_device_ = new AudioDevice(
          output_buffer_size,
          output_channels,
          output_sample_rate,
          this);
    }
    DCHECK(audio_input_device_);
    DCHECK(audio_output_device_);

    input_buffer_.reset(new int16[kMaxBufferSize * kMaxChannels]);
    output_buffer_.reset(new int16[kMaxBufferSize * kMaxChannels]);

    bytes_per_sample_ = sizeof(*input_buffer_.get());
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

void WebRtcAudioDeviceImpl::Render(
    const std::vector<float*>& audio_data,
    size_t number_of_frames,
    size_t audio_delay_milliseconds) {
  DCHECK_LE(number_of_frames, kMaxBufferSize);

  // Store the reported audio delay locally.
  output_delay_ms_ = audio_delay_milliseconds;

  const int channels = audio_data.size();
  DCHECK_LE(channels, kMaxChannels);

  const int samples_per_sec = static_cast<int>(input_sample_rate_);
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
  DCHECK_LE(number_of_frames, kMaxBufferSize);

  // Store the reported audio delay locally.
  input_delay_ms_ = audio_delay_milliseconds;

  const int channels = audio_data.size();
  DCHECK_LE(channels, kMaxChannels);
  uint32_t new_mic_level = 0;

  // Interleave, scale, and clip input to int16 and store result in
  // a local byte buffer.
  media::InterleaveFloatToInt16(audio_data,
                                input_buffer_.get(),
                                number_of_frames);

  const int samples_per_sec = static_cast<int>(output_sample_rate_);
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
  VLOG(1) << "RegisterEventObserver()";
  NOTIMPLEMENTED();
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
  if (initialized_)
    return 0;
  initialized_ = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::Terminate() {
  VLOG(1) << "Terminate()";
  if (!initialized_)
    return 0;
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
  VLOG(1) << "SetPlayoutDevice(index=" << index << ")";
  NOTIMPLEMENTED();
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetPlayoutDevice(WindowsDeviceType device) {
  VLOG(1) << "SetPlayoutDevice(device=" << device << ")";
  NOTIMPLEMENTED();
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetRecordingDevice(uint16_t index) {
  VLOG(1) << "SetRecordingDevice(index=" << index << ")";
  NOTIMPLEMENTED();
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetRecordingDevice(WindowsDeviceType device) {
  VLOG(1) << "SetRecordingDevice(device=" << device << ")";
  NOTIMPLEMENTED();
  return 0;
}

int32_t WebRtcAudioDeviceImpl::PlayoutIsAvailable(bool* available) {
  VLOG(1) << "PlayoutIsAvailable()";
  *available = (audio_output_device_ != NULL);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::InitPlayout() {
  VLOG(1) << "InitPlayout()";
  NOTIMPLEMENTED();
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
  VLOG(1) << "InitRecording()";
  NOTIMPLEMENTED();
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
  LOG_IF(ERROR, !audio_transport_callback_) << "Audio transport is missing";
  if (!audio_transport_callback_) {
    LOG(ERROR) << "Audio transport is missing";
    return -1;
  }
  if (recording_) {
    // webrtc::VoiceEngine assumes that it is OK to call Start() twice and
    // that the call is ignored the second time.
    LOG(WARNING) << "Recording is already active";
    return 0;
  }
  audio_input_device_->Start();
  recording_ = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopRecording() {
  VLOG(1) << "StopRecording()";
  DCHECK(audio_input_device_);
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
  NOTIMPLEMENTED();
  return -1;
}

bool WebRtcAudioDeviceImpl::AGC() const {
  NOTIMPLEMENTED();
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
  VLOG(1) << "SpeakerIsAvailable()";
  NOTIMPLEMENTED();
  *available = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::InitSpeaker() {
  VLOG(1) << "InitSpeaker()";
  NOTIMPLEMENTED();
  return 0;
}

bool WebRtcAudioDeviceImpl::SpeakerIsInitialized() const {
  NOTIMPLEMENTED();
  return true;
}

int32_t WebRtcAudioDeviceImpl::MicrophoneIsAvailable(bool* available) {
  VLOG(1) << "MicrophoneIsAvailable()";
  NOTIMPLEMENTED();
  *available = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::InitMicrophone() {
  VLOG(1) << "InitMicrophone()";
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::MinMicrophoneVolume(
    uint32_t* min_volume) const {
  NOTIMPLEMENTED();
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
  VLOG(1) << "StereoPlayoutIsAvailable()";
  NOTIMPLEMENTED();
  *available = false;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetStereoPlayout(bool enable) {
  VLOG(1) << "SetStereoPlayout(enable=" << enable << ")";
  NOTIMPLEMENTED();
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StereoPlayout(bool* enabled) const {
  NOTIMPLEMENTED();
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StereoRecordingIsAvailable(
    bool* available) const {
  VLOG(1) << "StereoRecordingIsAvailable()";
  NOTIMPLEMENTED();
  return 0;
}

int32_t WebRtcAudioDeviceImpl::SetStereoRecording(bool enable) {
  VLOG(1) << "SetStereoRecording(enable=" << enable << ")";
  NOTIMPLEMENTED();
  return -1;
}

int32_t WebRtcAudioDeviceImpl::StereoRecording(bool* enabled) const {
  NOTIMPLEMENTED();
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

bool WebRtcAudioDeviceImpl::BufferSizeIsValid(
    size_t buffer_size, float sample_rate) const {
  const int samples_per_sec = static_cast<int>(sample_rate);
  const int samples_per_10_msec = (samples_per_sec / 100);
  bool size_is_valid = (((buffer_size % samples_per_10_msec) == 0) &&
                         (buffer_size <= kMaxBufferSize));
  DLOG_IF(WARNING, !size_is_valid) << "Size of buffer must be and even "
                                   << "multiple of 10 ms and less than "
                                   << kMaxBufferSize;
  return size_is_valid;
}
