// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_device_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/win/windows_version.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/audio_hardware.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/audio_util.h"
#include "media/audio/sample_rates.h"

using content::AudioDeviceFactory;
using media::AudioParameters;

static const int64 kMillisecondsBetweenProcessCalls = 5000;
static const double kMaxVolumeLevel = 255.0;

// Supported hardware sample rates for input and output sides.
#if defined(OS_WIN) || defined(OS_MACOSX)
// media::GetAudioInput[Output]HardwareSampleRate() asks the audio layer
// for its current sample rate (set by the user) on Windows and Mac OS X.
// The listed rates below adds restrictions and WebRtcAudioDeviceImpl::Init()
// will fail if the user selects any rate outside these ranges.
static int kValidInputRates[] = {96000, 48000, 44100, 32000, 16000, 8000};
static int kValidOutputRates[] = {96000, 48000, 44100};
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
// media::GetAudioInput[Output]HardwareSampleRate() is hardcoded to return
// 48000 in both directions on Linux.
static int kValidInputRates[] = {48000};
static int kValidOutputRates[] = {48000};
#endif

namespace {

// Helper enum used for histogramming buffer sizes expressed in number of
// audio frames. This enumerator covers all supported sizes for all platforms.
// Example: k480 <=> 480 audio frames <=> 10ms@48kHz.
// TODO(henrika): can be moved to the media namespace if more clients need it.
// TODO(henrika): add support for k80 as well. Will be listed as unexpected for
// now. Very rare case though and most likeley only on Mac OS X.
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

enum HistogramDirection {
  kAudioOutput,
  kAudioInput
};

}  // anonymous namespace

// Helper method to convert integral values to their respective enum values
// above, or kUnexpectedAudioBufferSize if no match exists.
// TODO(henrika): add support for k80 as well given that 8000Hz input now has
// been added.
static AudioFramesPerBuffer AsAudioFramesPerBuffer(int frames_per_buffer) {
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

// Helper method which adds histogram data to be uploaded as part of an
// UMA logging event. Names: "WebRTC.Audio[Output|Input]SampleRate".
static void AddHistogramSampleRate(HistogramDirection dir, int param) {
  media::AudioSampleRate asr = media::AsAudioSampleRate(param);
  if (asr != media::kUnexpectedAudioSampleRate) {
    if (dir == kAudioOutput) {
      UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioOutputSampleRate",
                                asr, media::kUnexpectedAudioSampleRate);
    } else {
      UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputSampleRate",
                                asr, media::kUnexpectedAudioSampleRate);
    }
  } else {
    // Report unexpected sample rates using a unique histogram name.
    if (dir == kAudioOutput) {
      UMA_HISTOGRAM_COUNTS("WebRTC.AudioOutputSampleRateUnexpected", param);
    } else {
      UMA_HISTOGRAM_COUNTS("WebRTC.AudioInputSampleRateUnexpected", param);
    }
  }
}

// Helper method which adds histogram data to be uploaded as part of an
// UMA logging event. Names: "WebRTC.Audio[Output|Input]FramesPerBuffer".
static void AddHistogramFramesPerBuffer(HistogramDirection dir, int param) {
  AudioFramesPerBuffer afpb = AsAudioFramesPerBuffer(param);
  if (afpb != kUnexpectedAudioBufferSize) {
    if (dir == kAudioOutput) {
      UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioOutputFramesPerBuffer",
                                afpb, kUnexpectedAudioBufferSize);
    } else {
      UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputFramesPerBuffer",
                                afpb, kUnexpectedAudioBufferSize);
    }
  } else {
    // Report unexpected sample rates using a unique histogram name.
    if (dir == kAudioOutput) {
      UMA_HISTOGRAM_COUNTS("WebRTC.AudioOutputFramesPerBufferUnexpected",
                           param);
    } else {
      UMA_HISTOGRAM_COUNTS("WebRTC.AudioInputFramesPerBufferUnexpected", param);
    }
  }
}

WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()
    : ref_count_(0),
      render_loop_(base::MessageLoopProxy::current()),
      audio_transport_callback_(NULL),
      input_delay_ms_(0),
      output_delay_ms_(0),
      last_error_(AudioDeviceModule::kAdmErrNone),
      last_process_time_(base::TimeTicks::Now()),
      session_id_(0),
      bytes_per_sample_(0),
      initialized_(false),
      playing_(false),
      recording_(false),
      agc_is_enabled_(false) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()";
  // TODO(henrika): remove this restriction when factory is used for the
  // input side as well.
  DCHECK(RenderThreadImpl::current()) <<
      "WebRtcAudioDeviceImpl must be constructed on the render thread";
  audio_output_device_ = AudioDeviceFactory::NewOutputDevice();
  DCHECK(audio_output_device_);
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

int WebRtcAudioDeviceImpl::Render(
    media::AudioBus* audio_bus,
    int audio_delay_milliseconds) {
  DCHECK_LE(audio_bus->frames(), output_buffer_size());

  {
    base::AutoLock auto_lock(lock_);
    // Store the reported audio delay locally.
    output_delay_ms_ = audio_delay_milliseconds;
  }

  const int channels = audio_bus->channels();
  DCHECK_LE(channels, output_channels());

  int samples_per_sec = output_sample_rate();
  if (samples_per_sec == 44100) {
    // Even if the hardware runs at 44.1kHz, we use 44.0 internally.
    samples_per_sec = 44000;
  }
  int samples_per_10_msec = (samples_per_sec / 100);
  const int bytes_per_10_msec =
      channels * samples_per_10_msec * bytes_per_sample_;

  uint32_t num_audio_samples = 0;
  int accumulated_audio_samples = 0;

  char* audio_byte_buffer = reinterpret_cast<char*>(output_buffer_.get());

  // Get audio samples in blocks of 10 milliseconds from the registered
  // webrtc::AudioTransport source. Keep reading until our internal buffer
  // is full.
  while (accumulated_audio_samples < audio_bus->frames()) {
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
  audio_bus->FromInterleaved(output_buffer_.get(), audio_bus->frames(),
                             bytes_per_sample_);
  return audio_bus->frames();
}

void WebRtcAudioDeviceImpl::OnRenderError() {
  DCHECK_EQ(MessageLoop::current(), ChildProcess::current()->io_message_loop());
  // TODO(henrika): Implement error handling.
  LOG(ERROR) << "OnRenderError()";
}

void WebRtcAudioDeviceImpl::Capture(media::AudioBus* audio_bus,
                                    int audio_delay_milliseconds,
                                    double volume) {
  DCHECK_LE(audio_bus->frames(), input_buffer_size());
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

  const int channels = audio_bus->channels();
  DCHECK_LE(channels, input_channels());
  uint32_t new_mic_level = 0;

  // Interleave, scale, and clip input to int and store result in
  // a local byte buffer.
  audio_bus->ToInterleaved(audio_bus->frames(),
                           input_audio_parameters_.bits_per_sample() / 8,
                           input_buffer_.get());

  int samples_per_sec = input_sample_rate();
  if (samples_per_sec == 44100) {
    // Even if the hardware runs at 44.1kHz, we use 44.0 internally.
    samples_per_sec = 44000;
  }
  const int samples_per_10_msec = (samples_per_sec / 100);
  const int bytes_per_10_msec =
      channels * samples_per_10_msec * bytes_per_sample_;
  int accumulated_audio_samples = 0;

  char* audio_byte_buffer = reinterpret_cast<char*>(input_buffer_.get());

  // Map internal volume range of [0.0, 1.0] into [0, 255] used by the
  // webrtc::VoiceEngine.
  uint32_t current_mic_level = static_cast<uint32_t>(volume * kMaxVolumeLevel);

  // Write audio samples in blocks of 10 milliseconds to the registered
  // webrtc::AudioTransport sink. Keep writing until our internal byte
  // buffer is empty.
  while (accumulated_audio_samples < audio_bus->frames()) {
    // Deliver 10ms of recorded 16-bit linear PCM audio.
    audio_transport_callback_->RecordedDataIsAvailable(
        audio_byte_buffer,
        samples_per_10_msec,
        bytes_per_sample_,
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

void WebRtcAudioDeviceImpl::OnCaptureError() {
  DCHECK_EQ(MessageLoop::current(), ChildProcess::current()->io_message_loop());
  // TODO(henrika): Implement error handling.
  LOG(ERROR) << "OnCaptureError()";
}

void WebRtcAudioDeviceImpl::OnDeviceStarted(const std::string& device_id) {
  DVLOG(1) << "OnDeviceStarted (device_id=" << device_id << ")";
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

  // TODO(henrika): After switching to using the AudioDeviceFactory for
  // instantiating the input device, maybe this isn't a requirement anymore?
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
  DCHECK(!input_buffer_.get());
  DCHECK(!output_buffer_.get());

  // TODO(henrika): it could be possible to allow one of the directions (input
  // or output) to use a non-supported rate. As an example: if only the
  // output rate is OK, we could finalize Init() and only set up an
  // AudioOutputDevice.

  // Ask the browser for the default audio output hardware sample-rate.
  // This request is based on a synchronous IPC message.
  int out_sample_rate = audio_hardware::GetOutputSampleRate();
  DVLOG(1) << "Audio output hardware sample rate: " << out_sample_rate;
  AddHistogramSampleRate(kAudioOutput, out_sample_rate);

  // Verify that the reported output hardware sample rate is supported
  // on the current platform.
  if (std::find(&kValidOutputRates[0],
                &kValidOutputRates[0] + arraysize(kValidOutputRates),
                out_sample_rate) ==
      &kValidOutputRates[arraysize(kValidOutputRates)]) {
    DLOG(ERROR) << out_sample_rate << " is not a supported output rate.";
    return -1;
  }

  // Ask the browser for the default audio input hardware sample-rate.
  // This request is based on a synchronous IPC message.
  int in_sample_rate = audio_hardware::GetInputSampleRate();
  DVLOG(1) << "Audio input hardware sample rate: " << in_sample_rate;
  AddHistogramSampleRate(kAudioInput, in_sample_rate);

  // Verify that the reported input hardware sample rate is supported
  // on the current platform.
  if (std::find(&kValidInputRates[0],
                &kValidInputRates[0] + arraysize(kValidInputRates),
                in_sample_rate) ==
      &kValidInputRates[arraysize(kValidInputRates)]) {
    DLOG(ERROR) << in_sample_rate << " is not a supported input rate.";
    return -1;
  }

  // Ask the browser for the default number of audio input channels.
  // This request is based on a synchronous IPC message.
  ChannelLayout in_channel_layout = audio_hardware::GetInputChannelLayout();
  DVLOG(1) << "Audio input hardware channels: " << in_channel_layout;
  ChannelLayout out_channel_layout = CHANNEL_LAYOUT_MONO;

  AudioParameters::Format in_format = AudioParameters::AUDIO_PCM_LINEAR;
  int in_buffer_size = 0;
  int out_buffer_size = 0;

  // TODO(henrika): factor out all platform specific parts in separate
  // functions. Code is a bit messy right now.

// Windows
#if defined(OS_WIN)
  // Always use stereo rendering on Windows.
  out_channel_layout = CHANNEL_LAYOUT_STEREO;

  DVLOG(1) << "Using AUDIO_PCM_LOW_LATENCY as input mode on Windows.";
  in_format = AudioParameters::AUDIO_PCM_LOW_LATENCY;

  // Capture side: AUDIO_PCM_LOW_LATENCY is based on the Core Audio (WASAPI)
  // API which was introduced in Windows Vista. For lower Windows versions,
  // a callback-driven Wave implementation is used instead. An input buffer
  // size of 10ms works well for both these implementations.

  // Use different buffer sizes depending on the current hardware sample rate.
  if (in_sample_rate == 44100) {
    // We do run at 44.1kHz at the actual audio layer, but ask for frames
    // at 44.0kHz to ensure that we can feed them to the webrtc::VoiceEngine.
    in_buffer_size = 440;
  } else {
    in_buffer_size = (in_sample_rate / 100);
    DCHECK_EQ(in_buffer_size * 100, in_sample_rate) <<
        "Sample rate not supported. Should have been caught in Init().";
  }

  // Render side: AUDIO_PCM_LOW_LATENCY is based on the Core Audio (WASAPI)
  // API which was introduced in Windows Vista. For lower Windows versions,
  // a callback-driven Wave implementation is used instead. An output buffer
  // size of 10ms works well for WASAPI but 30ms is needed for Wave.

  // Use different buffer sizes depending on the current hardware sample rate.
  if (out_sample_rate == 96000 || out_sample_rate == 48000) {
    out_buffer_size = (out_sample_rate / 100);
  } else {
    // We do run at 44.1kHz at the actual audio layer, but ask for frames
    // at 44.0kHz to ensure that we can feed them to the webrtc::VoiceEngine.
    // TODO(henrika): figure out why we seem to need 20ms here for glitch-
    // free audio.
    out_buffer_size = 2 * 440;
  }

  // Windows XP and lower can't cope with 10 ms output buffer size.
  // It must be extended to 30 ms (60 ms will be used internally by WaveOut).
  if (!media::IsWASAPISupported()) {
    out_buffer_size = 3 * out_buffer_size;
    DLOG(WARNING) << "Extending the output buffer size by a factor of three "
                  << "since Windows XP has been detected.";
  }

// Mac OS X
#elif defined(OS_MACOSX)
  out_channel_layout = CHANNEL_LAYOUT_MONO;

  DVLOG(1) << "Using AUDIO_PCM_LOW_LATENCY as input mode on Mac OS X.";
  in_format = AudioParameters::AUDIO_PCM_LOW_LATENCY;

  // Capture side: AUDIO_PCM_LOW_LATENCY on Mac OS X is based on a callback-
  // driven Core Audio implementation. Tests have shown that 10ms is a suitable
  // frame size to use, both for 48kHz and 44.1kHz.

  // Use different buffer sizes depending on the current hardware sample rate.
  if (in_sample_rate == 44100) {
    // We do run at 44.1kHz at the actual audio layer, but ask for frames
    // at 44.0kHz to ensure that we can feed them to the webrtc::VoiceEngine.
    in_buffer_size = 440;
  } else {
    in_buffer_size = (in_sample_rate / 100);
    DCHECK_EQ(in_buffer_size * 100, in_sample_rate) <<
        "Sample rate not supported. Should have been caught in Init().";
  }

  // Render side: AUDIO_PCM_LOW_LATENCY on Mac OS X is based on a callback-
  // driven Core Audio implementation. Tests have shown that 10ms is a suitable
  // frame size to use, both for 48kHz and 44.1kHz.

  // Use different buffer sizes depending on the current hardware sample rate.
  if (out_sample_rate == 48000) {
    out_buffer_size = 480;
  } else {
    // We do run at 44.1kHz at the actual audio layer, but ask for frames
    // at 44.0kHz to ensure that we can feed them to the webrtc::VoiceEngine.
    out_buffer_size = 440;
  }
// Linux
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
  in_channel_layout = CHANNEL_LAYOUT_STEREO;
  out_channel_layout = CHANNEL_LAYOUT_MONO;

  // Based on tests using the current ALSA implementation in Chrome, we have
  // found that the best combination is 20ms on the input side and 10ms on the
  // output side.
  // TODO(henrika): It might be possible to reduce the input buffer
  // size and reduce the delay even more.
  in_buffer_size = 2 * 480;
  out_buffer_size = 480;
#else
  DLOG(ERROR) << "Unsupported platform";
  return -1;
#endif

  // Store utilized parameters to ensure that we can check them
  // after a successful initialization.
  output_audio_parameters_.Reset(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, out_channel_layout,
      out_sample_rate, 16, out_buffer_size);

  input_audio_parameters_.Reset(
      in_format, in_channel_layout, in_sample_rate,
      16, in_buffer_size);

  // Create and configure the audio capturing client.
  audio_input_device_ = AudioDeviceFactory::NewInputDevice();
  audio_input_device_->Initialize(input_audio_parameters_, this, this);

  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioOutputChannelLayout",
                            out_channel_layout, CHANNEL_LAYOUT_MAX);
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputChannelLayout",
                            in_channel_layout, CHANNEL_LAYOUT_MAX);
  AddHistogramFramesPerBuffer(kAudioOutput, out_buffer_size);
  AddHistogramFramesPerBuffer(kAudioInput, in_buffer_size);

  // Configure the audio rendering client.
  audio_output_device_->Initialize(output_audio_parameters_, this);

  DCHECK(audio_input_device_);

  // Allocate local audio buffers based on the parameters above.
  // It is assumed that each audio sample contains 16 bits and each
  // audio frame contains one or two audio samples depending on the
  // number of channels.
  input_buffer_.reset(new int16[input_buffer_size() * input_channels()]);
  output_buffer_.reset(new int16[output_buffer_size() * output_channels()]);

  DCHECK(input_buffer_.get());
  DCHECK(output_buffer_.get());

  bytes_per_sample_ = sizeof(*input_buffer_.get());

  initialized_ = true;

  DVLOG(1) << "Capture parameters (size/channels/rate): ("
           << input_buffer_size() << "/" << input_channels() << "/"
           << input_sample_rate() << ")";
  DVLOG(1) << "Render parameters (size/channels/rate): ("
           << output_buffer_size() << "/" << output_channels() << "/"
           << output_sample_rate() << ")";
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
  DCHECK(input_buffer_.get());
  DCHECK(output_buffer_.get());

  // Release all resources allocated in Init().
  audio_input_device_ = NULL;
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
  *available = initialized();
  return 0;
}

int32_t WebRtcAudioDeviceImpl::InitPlayout() {
  DVLOG(2) << "WARNING: WebRtcAudioDeviceImpl::InitPlayout() "
           << "NOT IMPLEMENTED";
  return 0;
}

bool WebRtcAudioDeviceImpl::PlayoutIsInitialized() const {
  DVLOG(1) << "PlayoutIsInitialized()";
  return initialized();
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
  LOG_IF(ERROR, !audio_transport_callback_) << "Audio transport is missing";
  if (!audio_transport_callback_) {
    return -1;
  }

  if (playing_) {
    // webrtc::VoiceEngine assumes that it is OK to call Start() twice and
    // that the call is ignored the second time.
    return 0;
  }

  start_render_time_ = base::Time::Now();

  audio_output_device_->Start();
  playing_ = true;
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

  audio_output_device_->Stop();
  playing_ = false;
  return 0;
}

bool WebRtcAudioDeviceImpl::Playing() const {
  return playing_;
}

int32_t WebRtcAudioDeviceImpl::StartRecording() {
  DVLOG(1) << "StartRecording()";
  LOG_IF(ERROR, !audio_transport_callback_) << "Audio transport is missing";
  if (!audio_transport_callback_) {
    return -1;
  }

  if (session_id_ <= 0) {
    LOG(ERROR) << session_id_ << " is an invalid session id.";
    return -1;
  }

  base::AutoLock auto_lock(lock_);
  if (recording_) {
    // webrtc::VoiceEngine assumes that it is OK to call Start() twice and
    // that the call is ignored the second time.
    return 0;
  }

  start_capture_time_ = base::Time::Now();

  // Specify the session_id which is mapped to a certain device.
  audio_input_device_->SetDevice(session_id_);
  audio_input_device_->Start();
  recording_ = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopRecording() {
  DVLOG(1) << "StopRecording()";
  {
    base::AutoLock auto_lock(lock_);
    if (!recording_) {
      // webrtc::VoiceEngine assumes that it is OK to call Stop()
      // more than once.
      return 0;
    }
  }

  // Add histogram data to be uploaded as part of an UMA logging event.
  // This histogram keeps track of total recording times.
  if (!start_capture_time_.is_null()) {
    base::TimeDelta capture_time = base::Time::Now() - start_capture_time_;
    UMA_HISTOGRAM_LONG_TIMES("WebRTC.AudioCaptureTime", capture_time);
  }

  audio_input_device_->Stop();

  base::AutoLock auto_lock(lock_);
  recording_ = false;
  return 0;
}

bool WebRtcAudioDeviceImpl::Recording() const {
  return recording_;
}

int32_t WebRtcAudioDeviceImpl::SetAGC(bool enable) {
  DVLOG(1) <<  "SetAGC(enable=" << enable << ")";
  // The current implementation does not support changing the AGC state while
  // recording. Using this approach simplifies the design and it is also
  // inline with the  latest WebRTC standard.
  DCHECK(initialized_);
  DCHECK(!recording_) << "Unable to set AGC state while recording is active.";
  if (recording_) {
    return -1;
  }

  audio_input_device_->SetAutomaticGainControl(enable);
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
  DVLOG(1) << "SetMicrophoneVolume(" << volume << ")";
  if (volume > kMaxVolumeLevel)
    return -1;

  // WebRTC uses a range of [0, 255] to represent the level of the microphone
  // volume. The IPC channel between the renderer and browser process works
  // with doubles in the [0.0, 1.0] range and we have to compensate for that.
  double normalized_volume = static_cast<double>(volume / kMaxVolumeLevel);
  audio_input_device_->SetVolume(normalized_volume);
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

void WebRtcAudioDeviceImpl::SetSessionId(int session_id) {
  session_id_ = session_id;
}
