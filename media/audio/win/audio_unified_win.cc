// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_unified_win.h"

#include <Functiondiscoverykeys_devpkey.h>

#include "base/debug/trace_event.h"
#ifndef NDEBUG
#include "base/file_util.h"
#include "base/path_service.h"
#endif
#include "base/time/time.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/avrt_wrapper_win.h"
#include "media/audio/win/core_audio_util_win.h"

using base::win::ScopedComPtr;
using base::win::ScopedCOMInitializer;
using base::win::ScopedCoMem;

// Smoothing factor in exponential smoothing filter where 0 < alpha < 1.
// Larger values of alpha reduce the level of smoothing.
// See http://en.wikipedia.org/wiki/Exponential_smoothing for details.
static const double kAlpha = 0.1;

// Compute a rate compensation which always attracts us back to a specified
// target level over a period of |kCorrectionTimeSeconds|.
static const double kCorrectionTimeSeconds = 0.1;

#ifndef NDEBUG
// Max number of columns in the output text file |kUnifiedAudioDebugFileName|.
// See LogElementNames enumerator for details on what each column represents.
static const size_t kMaxNumSampleTypes = 4;

static const size_t kMaxNumParams = 2;

// Max number of rows in the output file |kUnifiedAudioDebugFileName|.
// Each row corresponds to one set of sample values for (approximately) the
// same time instant (stored in the first column).
static const size_t kMaxFileSamples = 10000;

// Name of output debug file used for off-line analysis of measurements which
// can be utilized for performance tuning of this class.
static const char kUnifiedAudioDebugFileName[] = "unified_win_debug.txt";

// Name of output debug file used for off-line analysis of measurements.
// This file will contain a list of audio parameters.
static const char kUnifiedAudioParamsFileName[] = "unified_win_params.txt";
#endif

typedef uint32 ChannelConfig;

// Retrieves an integer mask which corresponds to the channel layout the
// audio engine uses for its internal processing/mixing of shared-mode
// streams. This mask indicates which channels are present in the multi-
// channel stream. The least significant bit corresponds with the Front Left
// speaker, the next least significant bit corresponds to the Front Right
// speaker, and so on, continuing in the order defined in KsMedia.h.
// See http://msdn.microsoft.com/en-us/library/windows/hardware/ff537083(v=vs.85).aspx
// for more details.
static ChannelConfig GetChannelConfig(EDataFlow data_flow) {
  WAVEFORMATPCMEX format;
  return SUCCEEDED(media::CoreAudioUtil::GetDefaultSharedModeMixFormat(
                   data_flow, eConsole, &format)) ?
                   static_cast<int>(format.dwChannelMask) : 0;
}

// Use the acquired IAudioClock interface to derive a time stamp of the audio
// sample which is currently playing through the speakers.
static double SpeakerStreamPosInMilliseconds(IAudioClock* clock) {
  UINT64 device_frequency = 0, position = 0;
  if (FAILED(clock->GetFrequency(&device_frequency)) ||
      FAILED(clock->GetPosition(&position, NULL))) {
    return 0.0;
  }
  return base::Time::kMillisecondsPerSecond *
      (static_cast<double>(position) / device_frequency);
}

// Get a time stamp in milliseconds given number of audio frames in |num_frames|
// using the current sample rate |fs| as scale factor.
// Example: |num_frames| = 960 and |fs| = 48000 => 20 [ms].
static double CurrentStreamPosInMilliseconds(UINT64 num_frames, DWORD fs) {
  return base::Time::kMillisecondsPerSecond *
      (static_cast<double>(num_frames) / fs);
}

// Convert a timestamp in milliseconds to byte units given the audio format
// in |format|.
// Example: |ts_milliseconds| equals 10, sample rate is 48000 and frame size
// is 4 bytes per audio frame => 480 * 4 = 1920 [bytes].
static int MillisecondsToBytes(double ts_milliseconds,
                               const WAVEFORMATPCMEX& format) {
  double seconds = ts_milliseconds / base::Time::kMillisecondsPerSecond;
  return static_cast<int>(seconds * format.Format.nSamplesPerSec *
      format.Format.nBlockAlign + 0.5);
}

// Convert frame count to milliseconds given the audio format in |format|.
static double FrameCountToMilliseconds(int num_frames,
                                       const WAVEFORMATPCMEX& format) {
  return (base::Time::kMillisecondsPerSecond * num_frames) /
      static_cast<double>(format.Format.nSamplesPerSec);
}

namespace media {

WASAPIUnifiedStream::WASAPIUnifiedStream(AudioManagerWin* manager,
                                         const AudioParameters& params,
                                         const std::string& input_device_id)
    : creating_thread_id_(base::PlatformThread::CurrentId()),
      manager_(manager),
      params_(params),
      input_channels_(params.input_channels()),
      output_channels_(params.channels()),
      input_device_id_(input_device_id),
      share_mode_(CoreAudioUtil::GetShareMode()),
      opened_(false),
      volume_(1.0),
      output_buffer_size_frames_(0),
      input_buffer_size_frames_(0),
      endpoint_render_buffer_size_frames_(0),
      endpoint_capture_buffer_size_frames_(0),
      num_written_frames_(0),
      total_delay_ms_(0.0),
      total_delay_bytes_(0),
      source_(NULL),
      input_callback_received_(false),
      io_sample_rate_ratio_(1),
      target_fifo_frames_(0),
      average_delta_(0),
      fifo_rate_compensation_(1),
      update_output_delay_(false),
      capture_delay_ms_(0) {
  TRACE_EVENT0("audio", "WASAPIUnifiedStream::WASAPIUnifiedStream");
  VLOG(1) << "WASAPIUnifiedStream::WASAPIUnifiedStream()";
  DCHECK(manager_);

  VLOG(1) << "Input channels : " << input_channels_;
  VLOG(1) << "Output channels: " << output_channels_;
  VLOG(1) << "Sample rate    : " << params_.sample_rate();
  VLOG(1) << "Buffer size    : " << params.frames_per_buffer();

#ifndef NDEBUG
  input_time_stamps_.reset(new int64[kMaxFileSamples]);
  num_frames_in_fifo_.reset(new int[kMaxFileSamples]);
  resampler_margin_.reset(new int[kMaxFileSamples]);
  fifo_rate_comps_.reset(new double[kMaxFileSamples]);
  num_elements_.reset(new int[kMaxNumSampleTypes]);
  std::fill(num_elements_.get(), num_elements_.get() + kMaxNumSampleTypes, 0);
  input_params_.reset(new int[kMaxNumParams]);
  output_params_.reset(new int[kMaxNumParams]);
#endif

  DVLOG_IF(1, share_mode_ == AUDCLNT_SHAREMODE_EXCLUSIVE)
      << "Core Audio (WASAPI) EXCLUSIVE MODE is enabled.";

  // Load the Avrt DLL if not already loaded. Required to support MMCSS.
  bool avrt_init = avrt::Initialize();
  DCHECK(avrt_init) << "Failed to load the avrt.dll";

  // All events are auto-reset events and non-signaled initially.

  // Create the event which the audio engine will signal each time a buffer
  // has been recorded.
  capture_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));

  // Create the event which will be set in Stop() when straeming shall stop.
  stop_streaming_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
}

WASAPIUnifiedStream::~WASAPIUnifiedStream() {
  VLOG(1) << "WASAPIUnifiedStream::~WASAPIUnifiedStream()";
#ifndef NDEBUG
  base::FilePath data_file_name;
  PathService::Get(base::DIR_EXE, &data_file_name);
  data_file_name = data_file_name.AppendASCII(kUnifiedAudioDebugFileName);
  data_file_ = file_util::OpenFile(data_file_name, "wt");
  DVLOG(1) << ">> Output file " << data_file_name.value() << " is created.";

  size_t n = 0;
  size_t elements_to_write = *std::min_element(
      num_elements_.get(), num_elements_.get() + kMaxNumSampleTypes);
  while (n < elements_to_write) {
    fprintf(data_file_, "%I64d %d %d %10.9f\n",
        input_time_stamps_[n],
        num_frames_in_fifo_[n],
        resampler_margin_[n],
        fifo_rate_comps_[n]);
    ++n;
  }
  file_util::CloseFile(data_file_);

  base::FilePath param_file_name;
  PathService::Get(base::DIR_EXE, &param_file_name);
  param_file_name = param_file_name.AppendASCII(kUnifiedAudioParamsFileName);
  param_file_ = file_util::OpenFile(param_file_name, "wt");
  DVLOG(1) << ">> Output file " << param_file_name.value() << " is created.";
  fprintf(param_file_, "%d %d\n", input_params_[0], input_params_[1]);
  fprintf(param_file_, "%d %d\n", output_params_[0], output_params_[1]);
  file_util::CloseFile(param_file_);
#endif
}

bool WASAPIUnifiedStream::Open() {
  TRACE_EVENT0("audio", "WASAPIUnifiedStream::Open");
  DVLOG(1) << "WASAPIUnifiedStream::Open()";
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  if (opened_)
    return true;

  AudioParameters hw_output_params;
  HRESULT hr = CoreAudioUtil::GetPreferredAudioParameters(
      eRender, eConsole, &hw_output_params);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get preferred output audio parameters.";
    return false;
  }

  AudioParameters hw_input_params;
  if (input_device_id_ == AudioManagerBase::kDefaultDeviceId) {
    // Query native parameters for the default capture device.
    hr = CoreAudioUtil::GetPreferredAudioParameters(
        eCapture, eConsole, &hw_input_params);
  } else {
    // Query native parameters for the capture device given by
    // |input_device_id_|.
    hr = CoreAudioUtil::GetPreferredAudioParameters(
        input_device_id_, &hw_input_params);
  }
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get preferred input audio parameters.";
    return false;
  }

  // It is currently only possible to open up the output audio device using
  // the native number of channels.
  if (output_channels_ != hw_output_params.channels()) {
    LOG(ERROR) << "Audio device does not support requested output channels.";
    return false;
  }

  // It is currently only possible to open up the input audio device using
  // the native number of channels. If the client asks for a higher channel
  // count, we will do channel upmixing in this class. The most typical
  // example is that the client provides stereo but the hardware can only be
  // opened in mono mode. We will do mono to stereo conversion in this case.
  if (input_channels_ < hw_input_params.channels()) {
    LOG(ERROR) << "Audio device does not support requested input channels.";
    return false;
  } else if (input_channels_ > hw_input_params.channels()) {
    ChannelLayout input_layout =
        GuessChannelLayout(hw_input_params.channels());
    ChannelLayout output_layout = GuessChannelLayout(input_channels_);
    channel_mixer_.reset(new ChannelMixer(input_layout, output_layout));
    DVLOG(1) << "Remixing input channel layout from " << input_layout
             << " to " << output_layout << "; from "
             << hw_input_params.channels() << " channels to "
             << input_channels_;
  }

  if (hw_output_params.sample_rate() != params_.sample_rate()) {
    LOG(ERROR) << "Requested sample-rate: " << params_.sample_rate()
               << " must match the hardware sample-rate: "
               << hw_output_params.sample_rate();
    return false;
  }

  if (hw_output_params.frames_per_buffer() != params_.frames_per_buffer()) {
    LOG(ERROR) << "Requested buffer size: " << params_.frames_per_buffer()
               << " must match the hardware buffer size: "
               << hw_output_params.frames_per_buffer();
    return false;
  }

  // Set up WAVEFORMATPCMEX structures for input and output given the specified
  // audio parameters.
  SetIOFormats(hw_input_params, params_);

  // Create the input and output busses.
  input_bus_ = AudioBus::Create(
      hw_input_params.channels(), input_buffer_size_frames_);
  output_bus_ = AudioBus::Create(params_);

  // One extra bus is needed for the input channel mixing case.
  if (channel_mixer_) {
    DCHECK_LT(hw_input_params.channels(), input_channels_);
    // The size of the |channel_bus_| must be the same as the size of the
    // output bus to ensure that the channel manager can deal with both
    // resampled and non-resampled data as input.
    channel_bus_ = AudioBus::Create(
        input_channels_, params_.frames_per_buffer());
  }

  // Check if FIFO and resampling is required to match the input rate to the
  // output rate. If so, a special thread loop, optimized for this case, will
  // be used. This mode is also called varispeed mode.
  // Note that we can also use this mode when input and output rates are the
  // same but native buffer sizes differ (can happen if two different audio
  // devices are used). For this case, the resampler uses a target ratio of
  // 1.0 but SetRatio is called to compensate for clock-drift. The FIFO is
  // required to compensate for the difference in buffer sizes.
  // TODO(henrika): we could perhaps improve the performance for the second
  // case here by only using the FIFO and avoid resampling. Not sure how much
  // that would give and we risk not compensation for clock drift.
  if (hw_input_params.sample_rate() != params_.sample_rate() ||
      hw_input_params.frames_per_buffer() != params_.frames_per_buffer()) {
    DoVarispeedInitialization(hw_input_params, params_);
  }

  // Render side (event driven only in varispeed mode):

  ScopedComPtr<IAudioClient> audio_output_client =
      CoreAudioUtil::CreateDefaultClient(eRender, eConsole);
  if (!audio_output_client)
    return false;

  if (!CoreAudioUtil::IsFormatSupported(audio_output_client,
                                        share_mode_,
                                        &output_format_)) {
    return false;
  }

  if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
    // The |render_event_| will be NULL unless varispeed mode is utilized.
    hr = CoreAudioUtil::SharedModeInitialize(
        audio_output_client, &output_format_, render_event_.Get(),
        &endpoint_render_buffer_size_frames_);
  } else {
    // TODO(henrika): add support for AUDCLNT_SHAREMODE_EXCLUSIVE.
  }
  if (FAILED(hr))
    return false;

  ScopedComPtr<IAudioRenderClient> audio_render_client =
      CoreAudioUtil::CreateRenderClient(audio_output_client);
  if (!audio_render_client)
    return false;

  // Capture side (always event driven but format depends on varispeed or not):

  ScopedComPtr<IAudioClient> audio_input_client;
  if (input_device_id_ == AudioManagerBase::kDefaultDeviceId) {
    audio_input_client = CoreAudioUtil::CreateDefaultClient(eCapture, eConsole);
  } else {
    ScopedComPtr<IMMDevice> audio_input_device(
      CoreAudioUtil::CreateDevice(input_device_id_));
    audio_input_client = CoreAudioUtil::CreateClient(audio_input_device);
  }
  if (!audio_input_client)
    return false;

  if (!CoreAudioUtil::IsFormatSupported(audio_input_client,
                                        share_mode_,
                                        &input_format_)) {
    return false;
  }

  if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
    // Include valid event handle for event-driven initialization.
    // The input side is always event driven independent of if varispeed is
    // used or not.
    hr = CoreAudioUtil::SharedModeInitialize(
        audio_input_client, &input_format_, capture_event_.Get(),
        &endpoint_capture_buffer_size_frames_);
  } else {
    // TODO(henrika): add support for AUDCLNT_SHAREMODE_EXCLUSIVE.
  }
  if (FAILED(hr))
    return false;

  ScopedComPtr<IAudioCaptureClient> audio_capture_client =
      CoreAudioUtil::CreateCaptureClient(audio_input_client);
  if (!audio_capture_client)
    return false;

  // Varispeed mode requires additional preparations.
  if (VarispeedMode())
    ResetVarispeed();

  // Store all valid COM interfaces.
  audio_output_client_ = audio_output_client;
  audio_render_client_ = audio_render_client;
  audio_input_client_ = audio_input_client;
  audio_capture_client_ = audio_capture_client;

  opened_ = true;
  return SUCCEEDED(hr);
}

void WASAPIUnifiedStream::Start(AudioSourceCallback* callback) {
  TRACE_EVENT0("audio", "WASAPIUnifiedStream::Start");
  DVLOG(1) << "WASAPIUnifiedStream::Start()";
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  CHECK(callback);
  CHECK(opened_);

  if (audio_io_thread_) {
    CHECK_EQ(callback, source_);
    return;
  }

  source_ = callback;

  if (VarispeedMode()) {
    ResetVarispeed();
    fifo_rate_compensation_ = 1.0;
    average_delta_ = 0.0;
    input_callback_received_ = false;
    update_output_delay_ = false;
  }

  // Create and start the thread that will listen for capture events.
  // We will also listen on render events on the same thread if varispeed
  // mode is utilized.
  audio_io_thread_.reset(
      new base::DelegateSimpleThread(this, "wasapi_io_thread"));
  audio_io_thread_->Start();
  if (!audio_io_thread_->HasBeenStarted()) {
    DLOG(ERROR) << "Failed to start WASAPI IO thread.";
    return;
  }

  // Start input streaming data between the endpoint buffer and the audio
  // engine.
  HRESULT hr = audio_input_client_->Start();
  if (FAILED(hr)) {
    StopAndJoinThread(hr);
    return;
  }

  // Ensure that the endpoint buffer is prepared with silence.
  if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
    if (!CoreAudioUtil::FillRenderEndpointBufferWithSilence(
             audio_output_client_, audio_render_client_)) {
      DLOG(WARNING) << "Failed to prepare endpoint buffers with silence.";
      return;
    }
  }
  num_written_frames_ = endpoint_render_buffer_size_frames_;

  // Start output streaming data between the endpoint buffer and the audio
  // engine.
  hr = audio_output_client_->Start();
  if (FAILED(hr)) {
    StopAndJoinThread(hr);
    return;
  }
}

void WASAPIUnifiedStream::Stop() {
  TRACE_EVENT0("audio", "WASAPIUnifiedStream::Stop");
  DVLOG(1) << "WASAPIUnifiedStream::Stop()";
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  if (!audio_io_thread_)
    return;

  // Stop input audio streaming.
  HRESULT hr = audio_input_client_->Stop();
  if (FAILED(hr)) {
    DLOG_IF(ERROR, hr != AUDCLNT_E_NOT_INITIALIZED)
      << "Failed to stop input streaming: " << std::hex << hr;
  }

  // Stop output audio streaming.
  hr = audio_output_client_->Stop();
  if (FAILED(hr)) {
    DLOG_IF(ERROR, hr != AUDCLNT_E_NOT_INITIALIZED)
        << "Failed to stop output streaming: " << std::hex << hr;
  }

  // Wait until the thread completes and perform cleanup.
  SetEvent(stop_streaming_event_.Get());
  audio_io_thread_->Join();
  audio_io_thread_.reset();

  // Ensure that we don't quit the main thread loop immediately next
  // time Start() is called.
  ResetEvent(stop_streaming_event_.Get());

  // Clear source callback, it'll be set again on the next Start() call.
  source_ = NULL;

  // Flush all pending data and reset the audio clock stream position to 0.
  hr = audio_output_client_->Reset();
  if (FAILED(hr)) {
    DLOG_IF(ERROR, hr != AUDCLNT_E_NOT_INITIALIZED)
        << "Failed to reset output streaming: " << std::hex << hr;
  }

  audio_input_client_->Reset();
  if (FAILED(hr)) {
    DLOG_IF(ERROR, hr != AUDCLNT_E_NOT_INITIALIZED)
        << "Failed to reset input streaming: " << std::hex << hr;
  }

  // Extra safety check to ensure that the buffers are cleared.
  // If the buffers are not cleared correctly, the next call to Start()
  // would fail with AUDCLNT_E_BUFFER_ERROR at IAudioRenderClient::GetBuffer().
  // TODO(henrika): this check is is only needed for shared-mode streams.
  UINT32 num_queued_frames = 0;
  audio_output_client_->GetCurrentPadding(&num_queued_frames);
  DCHECK_EQ(0u, num_queued_frames);
}

void WASAPIUnifiedStream::Close() {
  TRACE_EVENT0("audio", "WASAPIUnifiedStream::Close");
  DVLOG(1) << "WASAPIUnifiedStream::Close()";
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);

  // It is valid to call Close() before calling open or Start().
  // It is also valid to call Close() after Start() has been called.
  Stop();

  // Inform the audio manager that we have been closed. This will cause our
  // destruction.
  manager_->ReleaseOutputStream(this);
}

void WASAPIUnifiedStream::SetVolume(double volume) {
  DVLOG(1) << "SetVolume(volume=" << volume << ")";
  if (volume < 0 || volume > 1)
    return;
  volume_ = volume;
}

void WASAPIUnifiedStream::GetVolume(double* volume) {
  DVLOG(1) << "GetVolume()";
  *volume = static_cast<double>(volume_);
}


void WASAPIUnifiedStream::ProvideInput(int frame_delay, AudioBus* audio_bus) {
  // TODO(henrika): utilize frame_delay?
  // A non-zero framed delay means multiple callbacks were necessary to
  // fulfill the requested number of frames.
  if (frame_delay > 0)
    DVLOG(3) << "frame_delay: " << frame_delay;

#ifndef NDEBUG
  resampler_margin_[num_elements_[RESAMPLER_MARGIN]] =
    fifo_->frames() - audio_bus->frames();
  num_elements_[RESAMPLER_MARGIN]++;
#endif

  if (fifo_->frames() < audio_bus->frames()) {
    DVLOG(ERROR) << "Not enough data in the FIFO ("
                 << fifo_->frames() << " < " << audio_bus->frames() << ")";
    audio_bus->Zero();
    return;
  }

  fifo_->Consume(audio_bus, 0, audio_bus->frames());
}

void WASAPIUnifiedStream::SetIOFormats(const AudioParameters& input_params,
                                       const AudioParameters& output_params) {
  for (int n = 0; n < 2; ++n) {
    const AudioParameters& params = (n == 0) ? input_params : output_params;
    WAVEFORMATPCMEX* xformat = (n == 0) ? &input_format_ : &output_format_;
    WAVEFORMATEX* format = &xformat->Format;

    // Begin with the WAVEFORMATEX structure that specifies the basic format.
    format->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    format->nChannels =  params.channels();
    format->nSamplesPerSec = params.sample_rate();
    format->wBitsPerSample = params.bits_per_sample();
    format->nBlockAlign = (format->wBitsPerSample / 8) * format->nChannels;
    format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
    format->cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

    // Add the parts which are unique to WAVE_FORMAT_EXTENSIBLE.
    // Note that we always open up using the native channel layout.
    (*xformat).Samples.wValidBitsPerSample = format->wBitsPerSample;
    (*xformat).dwChannelMask = (n == 0) ?
        GetChannelConfig(eCapture) : GetChannelConfig(eRender);
    (*xformat).SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
  }

  input_buffer_size_frames_ = input_params.frames_per_buffer();
  output_buffer_size_frames_ = output_params.frames_per_buffer();
  VLOG(1) << "#audio frames per input buffer : " << input_buffer_size_frames_;
  VLOG(1) << "#audio frames per output buffer: " << output_buffer_size_frames_;

#ifndef NDEBUG
  input_params_[0] = input_format_.Format.nSamplesPerSec;
  input_params_[1] = input_buffer_size_frames_;
  output_params_[0] = output_format_.Format.nSamplesPerSec;
  output_params_[1] = output_buffer_size_frames_;
#endif
}

void WASAPIUnifiedStream::DoVarispeedInitialization(
    const AudioParameters& input_params, const AudioParameters& output_params) {
  DVLOG(1) << "WASAPIUnifiedStream::DoVarispeedInitialization()";

  // A FIFO is required in this mode for input to output buffering.
  // Note that it will add some latency.
  fifo_.reset(new AudioFifo(input_params.channels(), kFifoSize));
  VLOG(1) << "Using FIFO of size " << fifo_->max_frames()
          << " (#channels=" << input_params.channels() << ")";

  // Create the multi channel resampler using the initial sample rate ratio.
  // We will call MultiChannelResampler::SetRatio() during runtime to
  // allow arbitrary combinations of input and output devices running off
  // different clocks and using different drivers, with potentially
  // differing sample-rates. Note that the requested block size is given by
  // the native input buffer size |input_buffer_size_frames_|.
  io_sample_rate_ratio_ = input_params.sample_rate() /
      static_cast<double>(output_params.sample_rate());
  DVLOG(2) << "io_sample_rate_ratio: " << io_sample_rate_ratio_;
  resampler_.reset(new MultiChannelResampler(
      input_params.channels(), io_sample_rate_ratio_, input_buffer_size_frames_,
      base::Bind(&WASAPIUnifiedStream::ProvideInput, base::Unretained(this))));
  VLOG(1) << "Resampling from " << input_params.sample_rate() << " to "
          << output_params.sample_rate();

  // The optimal number of frames we'd like to keep in the FIFO at all times.
  // The actual size will vary but the goal is to ensure that the average size
  // is given by this value.
  target_fifo_frames_ = kTargetFifoSafetyFactor * input_buffer_size_frames_;
  VLOG(1) << "Target FIFO size: " <<  target_fifo_frames_;

  // Create the event which the audio engine will signal each time it
  // wants an audio buffer to render.
  render_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));

  // Allocate memory for temporary audio bus used to store resampled input
  // audio.
  resampled_bus_ = AudioBus::Create(
      input_params.channels(), output_buffer_size_frames_);

  // Buffer initial silence corresponding to target I/O buffering.
  ResetVarispeed();
}

void WASAPIUnifiedStream::ResetVarispeed() {
  DCHECK(VarispeedMode());

  // Buffer initial silence corresponding to target I/O buffering.
  fifo_->Clear();
  scoped_ptr<AudioBus> silence =
      AudioBus::Create(input_format_.Format.nChannels,
                       target_fifo_frames_);
  silence->Zero();
  fifo_->Push(silence.get());
  resampler_->Flush();
}

void WASAPIUnifiedStream::Run() {
  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);

  // Increase the thread priority.
  audio_io_thread_->SetThreadPriority(base::kThreadPriority_RealtimeAudio);

  // Enable MMCSS to ensure that this thread receives prioritized access to
  // CPU resources.
  // TODO(henrika): investigate if it is possible to include these additional
  // settings in SetThreadPriority() as well.
  DWORD task_index = 0;
  HANDLE mm_task = avrt::AvSetMmThreadCharacteristics(L"Pro Audio",
                                                      &task_index);
  bool mmcss_is_ok =
      (mm_task && avrt::AvSetMmThreadPriority(mm_task, AVRT_PRIORITY_CRITICAL));
  if (!mmcss_is_ok) {
    // Failed to enable MMCSS on this thread. It is not fatal but can lead
    // to reduced QoS at high load.
    DWORD err = GetLastError();
    LOG(WARNING) << "Failed to enable MMCSS (error code=" << err << ").";
  }

  // The IAudioClock interface enables us to monitor a stream's data
  // rate and the current position in the stream. Allocate it before we
  // start spinning.
  ScopedComPtr<IAudioClock> audio_output_clock;
  HRESULT hr = audio_output_client_->GetService(
      __uuidof(IAudioClock), audio_output_clock.ReceiveVoid());
  LOG_IF(WARNING, FAILED(hr)) << "Failed to create IAudioClock: "
                              << std::hex << hr;

  bool streaming = true;
  bool error = false;

  HANDLE wait_array[3];
  size_t num_handles = 0;
  wait_array[num_handles++] = stop_streaming_event_;
  wait_array[num_handles++] = capture_event_;
  if (render_event_) {
    // One extra event handle is needed in varispeed mode.
    wait_array[num_handles++] = render_event_;
  }

  // Keep streaming audio until stop event is signaled.
  // Capture events are always used but render events are only active in
  // varispeed mode.
  while (streaming && !error) {
    // Wait for a close-down event, or a new capture event.
    DWORD wait_result = WaitForMultipleObjects(num_handles,
                                               wait_array,
                                               FALSE,
                                               INFINITE);
    switch (wait_result) {
      case WAIT_OBJECT_0 + 0:
        // |stop_streaming_event_| has been set.
        streaming = false;
        break;
      case WAIT_OBJECT_0 + 1:
        // |capture_event_| has been set
        if (VarispeedMode()) {
          ProcessInputAudio();
        } else {
          ProcessInputAudio();
          ProcessOutputAudio(audio_output_clock);
        }
        break;
      case WAIT_OBJECT_0 + 2:
        DCHECK(VarispeedMode());
        // |render_event_| has been set
        ProcessOutputAudio(audio_output_clock);
        break;
      default:
        error = true;
        break;
    }
  }

  if (streaming && error) {
    // Stop audio streaming since something has gone wrong in our main thread
    // loop. Note that, we are still in a "started" state, hence a Stop() call
    // is required to join the thread properly.
    audio_input_client_->Stop();
    audio_output_client_->Stop();
    PLOG(ERROR) << "WASAPI streaming failed.";
  }

  // Disable MMCSS.
  if (mm_task && !avrt::AvRevertMmThreadCharacteristics(mm_task)) {
    PLOG(WARNING) << "Failed to disable MMCSS";
  }
}

void WASAPIUnifiedStream::ProcessInputAudio() {
  TRACE_EVENT0("audio", "WASAPIUnifiedStream::ProcessInputAudio");

  BYTE* data_ptr = NULL;
  UINT32 num_captured_frames = 0;
  DWORD flags = 0;
  UINT64 device_position = 0;
  UINT64 capture_time_stamp = 0;

  const int bytes_per_sample = input_format_.Format.wBitsPerSample >> 3;

  base::TimeTicks now_tick = base::TimeTicks::HighResNow();

#ifndef NDEBUG
  if (VarispeedMode()) {
    input_time_stamps_[num_elements_[INPUT_TIME_STAMP]] =
        now_tick.ToInternalValue();
    num_elements_[INPUT_TIME_STAMP]++;
  }
#endif

  // Retrieve the amount of data in the capture endpoint buffer.
  // |endpoint_capture_time_stamp| is the value of the performance
  // counter at the time that the audio endpoint device recorded
  // the device position of the first audio frame in the data packet.
  HRESULT hr = audio_capture_client_->GetBuffer(&data_ptr,
                                                &num_captured_frames,
                                                &flags,
                                                &device_position,
                                                &capture_time_stamp);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get data from the capture buffer";
    return;
  }

  if (hr == AUDCLNT_S_BUFFER_EMPTY) {
    // The return coded is a success code but a new packet is *not* available
    // and none of the output parameters in the GetBuffer() call contains valid
    // values. Best we can do is to deliver silence and avoid setting
    // |input_callback_received_| since this only seems to happen for the
    // initial event(s) on some devices.
    input_bus_->Zero();
  } else {
    // Valid data has been recorded and it is now OK to set the flag which
    // informs the render side that capturing has started.
    input_callback_received_ = true;
  }

  if (num_captured_frames != 0) {
    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
      // Clear out the capture buffer since silence is reported.
      input_bus_->Zero();
    } else {
      // Store captured data in an audio bus after de-interleaving
      // the data to match the audio bus structure.
      input_bus_->FromInterleaved(
          data_ptr, num_captured_frames, bytes_per_sample);
    }
  }

  hr = audio_capture_client_->ReleaseBuffer(num_captured_frames);
  DLOG_IF(ERROR, FAILED(hr)) << "Failed to release capture buffer";

  // Buffer input into FIFO if varispeed mode is used. The render event
  // will drive resampling of this data to match the output side.
  if (VarispeedMode()) {
    int available_frames = fifo_->max_frames() - fifo_->frames();
    if (input_bus_->frames() <= available_frames) {
      fifo_->Push(input_bus_.get());
    }
#ifndef NDEBUG
    num_frames_in_fifo_[num_elements_[NUM_FRAMES_IN_FIFO]] =
        fifo_->frames();
    num_elements_[NUM_FRAMES_IN_FIFO]++;
#endif
  }

  // Save resource by not asking for new delay estimates each time.
  // These estimates are fairly stable and it is perfectly safe to only
  // sample at a rate of ~1Hz.
  // TODO(henrika): we might have to increase the update rate in varispeed
  // mode since the delay variations are higher in this mode.
  if ((now_tick - last_delay_sample_time_).InMilliseconds() >
      kTimeDiffInMillisecondsBetweenDelayMeasurements &&
      input_callback_received_) {
    // Calculate the estimated capture delay, i.e., the latency between
    // the recording time and the time we when we are notified about
    // the recorded data. Note that the capture time stamp is given in
    // 100-nanosecond (0.1 microseconds) units.
    base::TimeDelta diff =
      now_tick - base::TimeTicks::FromInternalValue(0.1 * capture_time_stamp);
    capture_delay_ms_ = diff.InMillisecondsF();

    last_delay_sample_time_ = now_tick;
    update_output_delay_ = true;
  }
}

void WASAPIUnifiedStream::ProcessOutputAudio(IAudioClock* audio_output_clock) {
  TRACE_EVENT0("audio", "WASAPIUnifiedStream::ProcessOutputAudio");

  if (!input_callback_received_) {
    if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
      if (!CoreAudioUtil::FillRenderEndpointBufferWithSilence(
              audio_output_client_, audio_render_client_))
        DLOG(WARNING) << "Failed to prepare endpoint buffers with silence.";
    }
    return;
  }

  // Rate adjusted resampling is required in varispeed mode. It means that
  // recorded audio samples will be read from the FIFO, resampled to match the
  // output sample-rate and then stored in |resampled_bus_|.
  if (VarispeedMode()) {
    // Calculate a varispeed rate scalar factor to compensate for drift between
    // input and output.  We use the actual number of frames still in the FIFO
    // compared with the ideal value of |target_fifo_frames_|.
    int delta = fifo_->frames() - target_fifo_frames_;

    // Average |delta| because it can jitter back/forth quite frequently
    // by +/- the hardware buffer-size *if* the input and output callbacks are
    // happening at almost exactly the same time.  Also, if the input and output
    // sample-rates are different then |delta| will jitter quite a bit due to
    // the rate conversion happening in the varispeed, plus the jittering of
    // the callbacks.  The average value is what's important here.
    // We use an exponential smoothing filter to reduce the variations.
    average_delta_ += kAlpha * (delta - average_delta_);

    // Compute a rate compensation which always attracts us back to the
    // |target_fifo_frames_| over a period of kCorrectionTimeSeconds.
    double correction_time_frames =
        kCorrectionTimeSeconds * output_format_.Format.nSamplesPerSec;
    fifo_rate_compensation_ =
        (correction_time_frames + average_delta_) / correction_time_frames;

#ifndef NDEBUG
    fifo_rate_comps_[num_elements_[RATE_COMPENSATION]] =
        fifo_rate_compensation_;
    num_elements_[RATE_COMPENSATION]++;
#endif

    // Adjust for FIFO drift.
    const double new_ratio = io_sample_rate_ratio_ * fifo_rate_compensation_;
    resampler_->SetRatio(new_ratio);
    // Get resampled input audio from FIFO where the size is given by the
    // output side.
    resampler_->Resample(resampled_bus_->frames(), resampled_bus_.get());
  }

  // Derive a new total delay estimate if the capture side has set the
  // |update_output_delay_| flag.
  if (update_output_delay_) {
    // Calculate the estimated render delay, i.e., the time difference
    // between the time when data is added to the endpoint buffer and
    // when the data is played out on the actual speaker.
    const double stream_pos = CurrentStreamPosInMilliseconds(
        num_written_frames_ + output_buffer_size_frames_,
        output_format_.Format.nSamplesPerSec);
    const double speaker_pos =
        SpeakerStreamPosInMilliseconds(audio_output_clock);
    const double render_delay_ms = stream_pos - speaker_pos;
    const double fifo_delay_ms = VarispeedMode() ?
      FrameCountToMilliseconds(target_fifo_frames_, input_format_) : 0;

    // Derive the total delay, i.e., the sum of the input and output
    // delays. Also convert the value into byte units. An extra FIFO delay
    // is added for varispeed usage cases.
    total_delay_ms_ = VarispeedMode() ?
      capture_delay_ms_ + render_delay_ms + fifo_delay_ms :
      capture_delay_ms_ + render_delay_ms;
    DVLOG(2) << "total_delay_ms   : " << total_delay_ms_;
    DVLOG(3) << " capture_delay_ms: " << capture_delay_ms_;
    DVLOG(3) << " render_delay_ms : " << render_delay_ms;
    DVLOG(3) << " fifo_delay_ms   : " << fifo_delay_ms;
    total_delay_bytes_ = MillisecondsToBytes(total_delay_ms_, output_format_);

    // Wait for new signal from the capture side.
    update_output_delay_ = false;
  }

  // Select source depending on if varispeed is utilized or not.
  // Also, the source might be the output of a channel mixer if channel mixing
  // is required to match the native input channels to the number of input
  // channels used by the client (given by |input_channels_| in this case).
  AudioBus* input_bus = VarispeedMode() ?
      resampled_bus_.get() : input_bus_.get();
  if (channel_mixer_) {
    DCHECK_EQ(input_bus->frames(), channel_bus_->frames());
    // Most common case is 1->2 channel upmixing.
    channel_mixer_->Transform(input_bus, channel_bus_.get());
    // Use the output from the channel mixer as new input bus.
    input_bus = channel_bus_.get();
  }

  // Prepare for rendering by calling OnMoreIOData().
  int frames_filled = source_->OnMoreIOData(
      input_bus,
      output_bus_.get(),
      AudioBuffersState(0, total_delay_bytes_));
  DCHECK_EQ(frames_filled, output_bus_->frames());

  // Keep track of number of rendered frames since we need it for
  // our delay calculations.
  num_written_frames_ += frames_filled;

  // Derive the the amount of available space in the endpoint buffer.
  // Avoid render attempt if there is no room for a captured packet.
  UINT32 num_queued_frames = 0;
  audio_output_client_->GetCurrentPadding(&num_queued_frames);
  if (endpoint_render_buffer_size_frames_ - num_queued_frames <
      output_buffer_size_frames_)
    return;

  // Grab all available space in the rendering endpoint buffer
  // into which the client can write a data packet.
  uint8* audio_data = NULL;
  HRESULT hr = audio_render_client_->GetBuffer(output_buffer_size_frames_,
                                               &audio_data);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to access render buffer";
    return;
  }

  const int bytes_per_sample = output_format_.Format.wBitsPerSample >> 3;

  // Convert the audio bus content to interleaved integer data using
  // |audio_data| as destination.
  output_bus_->Scale(volume_);
  output_bus_->ToInterleaved(
      output_buffer_size_frames_, bytes_per_sample, audio_data);

  // Release the buffer space acquired in the GetBuffer() call.
  audio_render_client_->ReleaseBuffer(output_buffer_size_frames_, 0);
  DLOG_IF(ERROR, FAILED(hr)) << "Failed to release render buffer";

  return;
}

void WASAPIUnifiedStream::HandleError(HRESULT err) {
  CHECK((started() && GetCurrentThreadId() == audio_io_thread_->tid()) ||
        (!started() && GetCurrentThreadId() == creating_thread_id_));
  NOTREACHED() << "Error code: " << std::hex << err;
  if (source_)
    source_->OnError(this);
}

void WASAPIUnifiedStream::StopAndJoinThread(HRESULT err) {
  CHECK(GetCurrentThreadId() == creating_thread_id_);
  DCHECK(audio_io_thread_.get());
  SetEvent(stop_streaming_event_.Get());
  audio_io_thread_->Join();
  audio_io_thread_.reset();
  HandleError(err);
}

}  // namespace media
