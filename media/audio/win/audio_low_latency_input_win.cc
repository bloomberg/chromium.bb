// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_low_latency_input_win.h"

#include <objbase.h>

#include <algorithm>
#include <cmath>
#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/avrt_wrapper_win.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/base/audio_block_fifo.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"

using base::win::ScopedCOMInitializer;

namespace media {

namespace {

bool IsSupportedFormatForConversion(const WAVEFORMATEX& format) {
  if (format.nSamplesPerSec < limits::kMinSampleRate ||
      format.nSamplesPerSec > limits::kMaxSampleRate) {
    return false;
  }

  switch (format.wBitsPerSample) {
    case 8:
    case 16:
    case 32:
      break;
    default:
      return false;
  }

  if (GuessChannelLayout(format.nChannels) == CHANNEL_LAYOUT_UNSUPPORTED) {
    LOG(ERROR) << "Hardware configuration not supported for audio conversion";
    return false;
  }

  return true;
}

}  // namespace

WASAPIAudioInputStream::WASAPIAudioInputStream(
    AudioManagerWin* manager,
    const AudioParameters& params,
    const std::string& device_id,
    const AudioManager::LogCallback& log_callback)
    : manager_(manager), device_id_(device_id), log_callback_(log_callback) {
  DCHECK(manager_);
  DCHECK(!device_id_.empty());
  DCHECK(!log_callback_.is_null());

  // Load the Avrt DLL if not already loaded. Required to support MMCSS.
  bool avrt_init = avrt::Initialize();
  DCHECK(avrt_init) << "Failed to load the Avrt.dll";

  // Set up the desired output format specified by the client.
  output_format_.wFormatTag = WAVE_FORMAT_PCM;
  output_format_.nChannels = params.channels();
  output_format_.nSamplesPerSec = params.sample_rate();
  output_format_.wBitsPerSample = params.bits_per_sample();
  output_format_.nBlockAlign =
      (output_format_.wBitsPerSample / 8) * output_format_.nChannels;
  output_format_.nAvgBytesPerSec =
      output_format_.nSamplesPerSec * output_format_.nBlockAlign;
  output_format_.cbSize = 0;

  // Set the input (capture) format to the desired output format. In most cases,
  // it will be used unchanged.
  input_format_ = output_format_;

  // Size in bytes of each audio frame.
  frame_size_ = input_format_.nBlockAlign;

  // Store size of audio packets which we expect to get from the audio
  // endpoint device in each capture event.
  packet_size_frames_ = params.GetBytesPerBuffer() / input_format_.nBlockAlign;
  packet_size_bytes_ = params.GetBytesPerBuffer();
  DVLOG(1) << "Number of bytes per audio frame  : " << frame_size_;
  DVLOG(1) << "Number of audio frames per packet: " << packet_size_frames_;

  // All events are auto-reset events and non-signaled initially.

  // Create the event which the audio engine will signal each time
  // a buffer becomes ready to be processed by the client.
  audio_samples_ready_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(audio_samples_ready_event_.IsValid());

  // Create the event which will be set in Stop() when capturing shall stop.
  stop_capture_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(stop_capture_event_.IsValid());
}

WASAPIAudioInputStream::~WASAPIAudioInputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool WASAPIAudioInputStream::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(OPEN_RESULT_OK, open_result_);

  // Verify that we are not already opened.
  if (opened_) {
    log_callback_.Run("WASAPIAIS::Open: already open");
    return false;
  }

  // Obtain a reference to the IMMDevice interface of the capturing
  // device with the specified unique identifier or role which was
  // set at construction.
  HRESULT hr = SetCaptureDevice();
  if (FAILED(hr)) {
    ReportOpenResult(hr);
    return false;
  }

  // Obtain an IAudioClient interface which enables us to create and initialize
  // an audio stream between an audio application and the audio engine.
  hr = endpoint_device_->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER,
                                  NULL, &audio_client_);
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_ACTIVATION_FAILED;
    ReportOpenResult(hr);
    return false;
  }

#ifndef NDEBUG
  // Retrieve the stream format which the audio engine uses for its internal
  // processing/mixing of shared-mode streams. This function call is for
  // diagnostic purposes only and only in debug mode.
  hr = GetAudioEngineStreamFormat();
#endif

  // Verify that the selected audio endpoint supports the specified format
  // set during construction.
  hr = S_OK;
  if (!DesiredFormatIsSupported(&hr)) {
    open_result_ = OPEN_RESULT_FORMAT_NOT_SUPPORTED;
    ReportOpenResult(hr);
    return false;
  }

  // Initialize the audio stream between the client and the device using
  // shared mode and a lowest possible glitch-free latency.
  hr = InitializeAudioEngine();
  if (SUCCEEDED(hr) && converter_)
    open_result_ = OPEN_RESULT_OK_WITH_RESAMPLING;
  ReportOpenResult(hr);  // Report before we assign a value to |opened_|.
  opened_ = SUCCEEDED(hr);

  return opened_;
}

void WASAPIAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  DLOG_IF(ERROR, !opened_) << "Open() has not been called successfully";
  if (!opened_)
    return;

  if (started_)
    return;

  if (device_id_ == AudioDeviceDescription::kLoopbackWithMuteDeviceId &&
      system_audio_volume_) {
    BOOL muted = false;
    system_audio_volume_->GetMute(&muted);

    // If the system audio is muted at the time of capturing, then no need to
    // mute it again, and later we do not unmute system audio when stopping
    // capturing.
    if (!muted) {
      system_audio_volume_->SetMute(true, NULL);
      mute_done_ = true;
    }
  }

  DCHECK(!sink_);
  sink_ = callback;

  // Starts periodic AGC microphone measurements if the AGC has been enabled
  // using SetAutomaticGainControl().
  StartAgc();

  // Create and start the thread that will drive the capturing by waiting for
  // capture events.
  DCHECK(!capture_thread_.get());
  capture_thread_.reset(new base::DelegateSimpleThread(
      this, "wasapi_capture_thread",
      base::SimpleThread::Options(base::ThreadPriority::REALTIME_AUDIO)));
  capture_thread_->Start();

  // Start streaming data between the endpoint buffer and the audio engine.
  HRESULT hr = audio_client_->Start();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to start input streaming.";
    log_callback_.Run(base::StringPrintf(
        "WASAPIAIS::Start: Failed to start audio client, hresult = %#lx", hr));
  }

  if (SUCCEEDED(hr) && audio_render_client_for_loopback_.Get()) {
    hr = audio_render_client_for_loopback_->Start();
    if (FAILED(hr))
      log_callback_.Run(base::StringPrintf(
          "WASAPIAIS::Start: Failed to start render client for loopback, "
          "hresult = %#lx",
          hr));
  }

  started_ = SUCCEEDED(hr);
}

void WASAPIAudioInputStream::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "WASAPIAudioInputStream::Stop()";
  if (!started_)
    return;

  // We have muted system audio for capturing, so we need to unmute it when
  // capturing stops.
  if (device_id_ == AudioDeviceDescription::kLoopbackWithMuteDeviceId &&
      mute_done_) {
    DCHECK(system_audio_volume_);
    if (system_audio_volume_) {
      system_audio_volume_->SetMute(false, NULL);
      mute_done_ = false;
    }
  }

  // Stops periodic AGC microphone measurements.
  StopAgc();

  // Shut down the capture thread.
  if (stop_capture_event_.IsValid()) {
    SetEvent(stop_capture_event_.Get());
  }

  // Stop the input audio streaming.
  HRESULT hr = audio_client_->Stop();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to stop input streaming.";
  }

  // Wait until the thread completes and perform cleanup.
  if (capture_thread_) {
    SetEvent(stop_capture_event_.Get());
    capture_thread_->Join();
    capture_thread_.reset();
  }

  started_ = false;
  sink_ = NULL;
}

void WASAPIAudioInputStream::Close() {
  DVLOG(1) << "WASAPIAudioInputStream::Close()";
  // It is valid to call Close() before calling open or Start().
  // It is also valid to call Close() after Start() has been called.
  Stop();

  if (converter_)
    converter_->RemoveInput(this);

  // Inform the audio manager that we have been closed. This will cause our
  // destruction.
  manager_->ReleaseInputStream(this);
}

double WASAPIAudioInputStream::GetMaxVolume() {
  // Verify that Open() has been called succesfully, to ensure that an audio
  // session exists and that an ISimpleAudioVolume interface has been created.
  DLOG_IF(ERROR, !opened_) << "Open() has not been called successfully";
  if (!opened_)
    return 0.0;

  // The effective volume value is always in the range 0.0 to 1.0, hence
  // we can return a fixed value (=1.0) here.
  return 1.0;
}

void WASAPIAudioInputStream::SetVolume(double volume) {
  DVLOG(1) << "SetVolume(volume=" << volume << ")";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(volume, 0.0);
  DCHECK_LE(volume, 1.0);

  DLOG_IF(ERROR, !opened_) << "Open() has not been called successfully";
  if (!opened_)
    return;

  // Set a new master volume level. Valid volume levels are in the range
  // 0.0 to 1.0. Ignore volume-change events.
  HRESULT hr =
      simple_audio_volume_->SetMasterVolume(static_cast<float>(volume), NULL);
  if (FAILED(hr))
    DLOG(WARNING) << "Failed to set new input master volume.";

  // Update the AGC volume level based on the last setting above. Note that,
  // the volume-level resolution is not infinite and it is therefore not
  // possible to assume that the volume provided as input parameter can be
  // used directly. Instead, a new query to the audio hardware is required.
  // This method does nothing if AGC is disabled.
  UpdateAgcVolume();
}

double WASAPIAudioInputStream::GetVolume() {
  DCHECK(opened_) << "Open() has not been called successfully";
  if (!opened_)
    return 0.0;

  // Retrieve the current volume level. The value is in the range 0.0 to 1.0.
  float level = 0.0f;
  HRESULT hr = simple_audio_volume_->GetMasterVolume(&level);
  if (FAILED(hr))
    DLOG(WARNING) << "Failed to get input master volume.";

  return static_cast<double>(level);
}

bool WASAPIAudioInputStream::IsMuted() {
  DCHECK(opened_) << "Open() has not been called successfully";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!opened_)
    return false;

  // Retrieves the current muting state for the audio session.
  BOOL is_muted = FALSE;
  HRESULT hr = simple_audio_volume_->GetMute(&is_muted);
  if (FAILED(hr))
    DLOG(WARNING) << "Failed to get input master volume.";

  return is_muted != FALSE;
}

void WASAPIAudioInputStream::Run() {
  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);

  // Enable MMCSS to ensure that this thread receives prioritized access to
  // CPU resources.
  DWORD task_index = 0;
  HANDLE mm_task =
      avrt::AvSetMmThreadCharacteristics(L"Pro Audio", &task_index);
  bool mmcss_is_ok =
      (mm_task && avrt::AvSetMmThreadPriority(mm_task, AVRT_PRIORITY_CRITICAL));
  if (!mmcss_is_ok) {
    // Failed to enable MMCSS on this thread. It is not fatal but can lead
    // to reduced QoS at high load.
    DWORD err = GetLastError();
    LOG(WARNING) << "Failed to enable MMCSS (error code=" << err << ").";
  }

  // Allocate a buffer with a size that enables us to take care of cases like:
  // 1) The recorded buffer size is smaller, or does not match exactly with,
  //    the selected packet size used in each callback.
  // 2) The selected buffer size is larger than the recorded buffer size in
  //    each event.
  // In the case where no resampling is required, a single buffer should be
  // enough but in case we get buffers that don't match exactly, we'll go with
  // two. Same applies if we need to resample and the buffer ratio is perfect.
  // However if the buffer ratio is imperfect, we will need 3 buffers to safely
  // be able to buffer up data in cases where a conversion requires two audio
  // buffers (and we need to be able to write to the third one).
  size_t capture_buffer_size =
      std::max(2 * endpoint_buffer_size_frames_ * frame_size_,
               2 * packet_size_frames_ * frame_size_);
  int buffers_required = capture_buffer_size / packet_size_bytes_;
  if (converter_ && imperfect_buffer_size_conversion_)
    ++buffers_required;

  DCHECK(!fifo_);
  fifo_.reset(new AudioBlockFifo(input_format_.nChannels, packet_size_frames_,
                                 buffers_required));

  DVLOG(1) << "AudioBlockFifo buffer count: " << buffers_required;

  bool recording = true;
  bool error = false;
  double volume = GetVolume();
  HANDLE wait_array[2] = {stop_capture_event_.Get(),
                          audio_samples_ready_event_.Get()};

  Microsoft::WRL::ComPtr<IAudioClock> audio_clock;
  audio_client_->GetService(IID_PPV_ARGS(&audio_clock));
  if (!audio_clock)
    LOG(WARNING) << "IAudioClock unavailable, capture times may be inaccurate.";

  while (recording && !error) {
    HRESULT hr = S_FALSE;

    // Wait for a close-down event or a new capture event.
    DWORD wait_result = WaitForMultipleObjects(2, wait_array, FALSE, INFINITE);
    switch (wait_result) {
      case WAIT_FAILED:
        error = true;
        break;
      case WAIT_OBJECT_0 + 0:
        // |stop_capture_event_| has been set.
        recording = false;
        break;
      case WAIT_OBJECT_0 + 1: {
        TRACE_EVENT1("audio", "WASAPIAudioInputStream::Run_0", "sample rate",
                     input_format_.nSamplesPerSec);
        // |audio_samples_ready_event_| has been set.
        BYTE* data_ptr = NULL;
        UINT32 num_frames_to_read = 0;
        DWORD flags = 0;
        UINT64 device_position = 0;

        // Note: The units on this are 100ns intervals. Both GetBuffer() and
        // GetPosition() will handle the translation from the QPC value, so we
        // just need to convert from 100ns units into us. Which is just dividing
        // by 10.0 since 10x100ns = 1us.
        UINT64 capture_time_100ns = 0;

        // Retrieve the amount of data in the capture endpoint buffer,
        // replace it with silence if required, create callbacks for each
        // packet and store non-delivered data for the next event.
        hr = audio_capture_client_->GetBuffer(&data_ptr, &num_frames_to_read,
                                              &flags, &device_position,
                                              &capture_time_100ns);
        if (FAILED(hr)) {
          DLOG(ERROR) << "Failed to get data from the capture buffer";
          continue;
        }

        // TODO(dalecurtis, olka): Is this ever false?
        if (audio_clock) {
          // The reported timestamp from GetBuffer is not as reliable as the
          // clock from the client.  We've seen timestamps reported for
          // USB audio devices, be off by several days.  Furthermore we've
          // seen them jump back in time every 2 seconds or so.
          audio_clock->GetPosition(&device_position, &capture_time_100ns);
        }

        base::TimeTicks capture_time;
        if (capture_time_100ns) {
          // See conversion notes on |capture_time_100ns|.
          capture_time +=
              base::TimeDelta::FromMicroseconds(capture_time_100ns / 10.0);
        } else {
          // We may not have an IAudioClock or GetPosition() may return zero.
          capture_time = base::TimeTicks::Now();
        }

        // Adjust |capture_time| for the FIFO before pushing.
        capture_time -= AudioTimestampHelper::FramesToTime(
            fifo_->GetAvailableFrames(), input_format_.nSamplesPerSec);

        if (num_frames_to_read != 0) {
          if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
            fifo_->PushSilence(num_frames_to_read);
          } else {
            fifo_->Push(data_ptr, num_frames_to_read,
                        input_format_.wBitsPerSample / 8);
          }
        }

        hr = audio_capture_client_->ReleaseBuffer(num_frames_to_read);
        DLOG_IF(ERROR, FAILED(hr)) << "Failed to release capture buffer";

        // Get a cached AGC volume level which is updated once every second
        // on the audio manager thread. Note that, |volume| is also updated
        // each time SetVolume() is called through IPC by the render-side AGC.
        GetAgcVolume(&volume);

        // Deliver captured data to the registered consumer using a packet
        // size which was specified at construction.
        while (fifo_->available_blocks()) {
          if (converter_) {
            if (imperfect_buffer_size_conversion_ &&
                fifo_->available_blocks() == 1) {
              // Special case. We need to buffer up more audio before we can
              // convert or else we'll suffer an underrun.
              break;
            }
            converter_->Convert(convert_bus_.get());
            sink_->OnData(convert_bus_.get(), capture_time, volume);

            // Move the capture time forward for each vended block.
            capture_time += AudioTimestampHelper::FramesToTime(
                convert_bus_->frames(), output_format_.nSamplesPerSec);
          } else {
            sink_->OnData(fifo_->Consume(), capture_time, volume);

            // Move the capture time forward for each vended block.
            capture_time += AudioTimestampHelper::FramesToTime(
                packet_size_frames_, input_format_.nSamplesPerSec);
          }
        }
      } break;
      default:
        error = true;
        break;
    }
  }

  if (recording && error) {
    // TODO(henrika): perhaps it worth improving the cleanup here by e.g.
    // stopping the audio client, joining the thread etc.?
    NOTREACHED() << "WASAPI capturing failed with error code "
                 << GetLastError();
  }

  // Disable MMCSS.
  if (mm_task && !avrt::AvRevertMmThreadCharacteristics(mm_task)) {
    PLOG(WARNING) << "Failed to disable MMCSS";
  }

  fifo_.reset();
}

void WASAPIAudioInputStream::HandleError(HRESULT err) {
  NOTREACHED() << "Error code: " << err;
  if (sink_)
    sink_->OnError();
}

HRESULT WASAPIAudioInputStream::SetCaptureDevice() {
  DCHECK_EQ(OPEN_RESULT_OK, open_result_);
  DCHECK(!endpoint_device_.Get());

  Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr =
      ::CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
                         CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&enumerator));
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_CREATE_INSTANCE;
    return hr;
  }

  // Retrieve the IMMDevice by using the specified role or the specified
  // unique endpoint device-identification string.

  if (device_id_ == AudioDeviceDescription::kDefaultDeviceId) {
    // Retrieve the default capture audio endpoint for the specified role.
    // Note that, in Windows Vista, the MMDevice API supports device roles
    // but the system-supplied user interface programs do not.
    hr = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole,
                                             endpoint_device_.GetAddressOf());
  } else if (device_id_ == AudioDeviceDescription::kCommunicationsDeviceId) {
    hr = enumerator->GetDefaultAudioEndpoint(eCapture, eCommunications,
                                             endpoint_device_.GetAddressOf());
  } else if (device_id_ == AudioDeviceDescription::kLoopbackWithMuteDeviceId) {
    // Capture the default playback stream.
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                             endpoint_device_.GetAddressOf());

    if (SUCCEEDED(hr)) {
      endpoint_device_->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
                                 NULL, &system_audio_volume_);
    }
  } else if (device_id_ == AudioDeviceDescription::kLoopbackInputDeviceId) {
    // Capture the default playback stream.
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                             endpoint_device_.GetAddressOf());
  } else {
    hr = enumerator->GetDevice(base::UTF8ToUTF16(device_id_).c_str(),
                               endpoint_device_.GetAddressOf());
  }

  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_NO_ENDPOINT;
    return hr;
  }

  // Verify that the audio endpoint device is active, i.e., the audio
  // adapter that connects to the endpoint device is present and enabled.
  DWORD state = DEVICE_STATE_DISABLED;
  hr = endpoint_device_->GetState(&state);
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_NO_STATE;
    return hr;
  }

  if (!(state & DEVICE_STATE_ACTIVE)) {
    DLOG(ERROR) << "Selected capture device is not active.";
    open_result_ = OPEN_RESULT_DEVICE_NOT_ACTIVE;
    hr = E_ACCESSDENIED;
  }

  return hr;
}

HRESULT WASAPIAudioInputStream::GetAudioEngineStreamFormat() {
  HRESULT hr = S_OK;
#ifndef NDEBUG
  // The GetMixFormat() method retrieves the stream format that the
  // audio engine uses for its internal processing of shared-mode streams.
  // The method always uses a WAVEFORMATEXTENSIBLE structure, instead
  // of a stand-alone WAVEFORMATEX structure, to specify the format.
  // An WAVEFORMATEXTENSIBLE structure can specify both the mapping of
  // channels to speakers and the number of bits of precision in each sample.
  base::win::ScopedCoMem<WAVEFORMATEXTENSIBLE> format_ex;
  hr =
      audio_client_->GetMixFormat(reinterpret_cast<WAVEFORMATEX**>(&format_ex));

  // See http://msdn.microsoft.com/en-us/windows/hardware/gg463006#EFH
  // for details on the WAVE file format.
  WAVEFORMATEX format = format_ex->Format;
  DVLOG(2) << "WAVEFORMATEX:";
  DVLOG(2) << "  wFormatTags    : 0x" << std::hex << format.wFormatTag;
  DVLOG(2) << "  nChannels      : " << format.nChannels;
  DVLOG(2) << "  nSamplesPerSec : " << format.nSamplesPerSec;
  DVLOG(2) << "  nAvgBytesPerSec: " << format.nAvgBytesPerSec;
  DVLOG(2) << "  nBlockAlign    : " << format.nBlockAlign;
  DVLOG(2) << "  wBitsPerSample : " << format.wBitsPerSample;
  DVLOG(2) << "  cbSize         : " << format.cbSize;

  DVLOG(2) << "WAVEFORMATEXTENSIBLE:";
  DVLOG(2) << " wValidBitsPerSample: "
           << format_ex->Samples.wValidBitsPerSample;
  DVLOG(2) << " dwChannelMask      : 0x" << std::hex
           << format_ex->dwChannelMask;
  if (format_ex->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
    DVLOG(2) << " SubFormat          : KSDATAFORMAT_SUBTYPE_PCM";
  else if (format_ex->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
    DVLOG(2) << " SubFormat          : KSDATAFORMAT_SUBTYPE_IEEE_FLOAT";
  else if (format_ex->SubFormat == KSDATAFORMAT_SUBTYPE_WAVEFORMATEX)
    DVLOG(2) << " SubFormat          : KSDATAFORMAT_SUBTYPE_WAVEFORMATEX";
#endif
  return hr;
}

bool WASAPIAudioInputStream::DesiredFormatIsSupported(HRESULT* hr) {
  // An application that uses WASAPI to manage shared-mode streams can rely
  // on the audio engine to perform only limited format conversions. The audio
  // engine can convert between a standard PCM sample size used by the
  // application and the floating-point samples that the engine uses for its
  // internal processing. However, the format for an application stream
  // typically must have the same number of channels and the same sample
  // rate as the stream format used by the device.
  // Many audio devices support both PCM and non-PCM stream formats. However,
  // the audio engine can mix only PCM streams.
  base::win::ScopedCoMem<WAVEFORMATEX> closest_match;
  HRESULT hresult = audio_client_->IsFormatSupported(
      AUDCLNT_SHAREMODE_SHARED, &input_format_, &closest_match);
  DLOG_IF(ERROR, hresult == S_FALSE)
      << "Format is not supported but a closest match exists.";

  if (hresult == S_FALSE) {
    // Change the format we're going to ask for to better match with what the OS
    // can provide.  If we succeed in initializing the audio client in this
    // format and are able to convert from this format, we will do that
    // conversion.
    input_format_.nChannels = closest_match->nChannels;
    input_format_.nSamplesPerSec = closest_match->nSamplesPerSec;

    // If the closest match is fixed point PCM (WAVE_FORMAT_PCM or
    // KSDATAFORMAT_SUBTYPE_PCM), we use the closest match's bits per sample.
    // Otherwise, we keep the bits sample as is since we still request fixed
    // point PCM. In that case the closest match is typically in float format
    // (KSDATAFORMAT_SUBTYPE_IEEE_FLOAT).
    auto format_is_pcm = [](const WAVEFORMATEX* format) {
      if (format->wFormatTag == WAVE_FORMAT_PCM)
        return true;
      if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const WAVEFORMATEXTENSIBLE* format_ex =
            reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        return format_ex->SubFormat == KSDATAFORMAT_SUBTYPE_PCM;
      }
      return false;
    };
    if (format_is_pcm(closest_match))
      input_format_.wBitsPerSample = closest_match->wBitsPerSample;

    input_format_.nBlockAlign =
        (input_format_.wBitsPerSample / 8) * input_format_.nChannels;
    input_format_.nAvgBytesPerSec =
        input_format_.nSamplesPerSec * input_format_.nBlockAlign;

    if (IsSupportedFormatForConversion(input_format_)) {
      DVLOG(1) << "Will convert capture audio from: \nbits: "
               << input_format_.wBitsPerSample
               << "\nsample rate: " << input_format_.nSamplesPerSec
               << "\nchannels: " << input_format_.nChannels
               << "\nblock align: " << input_format_.nBlockAlign
               << "\navg bytes per sec: " << input_format_.nAvgBytesPerSec;

      // Ideally, we want a 1:1 ratio between the buffers we get and the buffers
      // we give to OnData so that each buffer we receive from the OS can be
      // directly converted to a buffer that matches with what was asked for.
      const double buffer_ratio = output_format_.nSamplesPerSec /
                                  static_cast<double>(packet_size_frames_);
      double new_frames_per_buffer =
          input_format_.nSamplesPerSec / buffer_ratio;

      const auto input_layout = GuessChannelLayout(input_format_.nChannels);
      DCHECK_NE(CHANNEL_LAYOUT_UNSUPPORTED, input_layout);
      const auto output_layout = GuessChannelLayout(output_format_.nChannels);
      DCHECK_NE(CHANNEL_LAYOUT_UNSUPPORTED, output_layout);

      const AudioParameters input(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  input_layout, input_format_.nSamplesPerSec,
                                  input_format_.wBitsPerSample,
                                  static_cast<int>(new_frames_per_buffer));

      const AudioParameters output(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                   output_layout, output_format_.nSamplesPerSec,
                                   output_format_.wBitsPerSample,
                                   packet_size_frames_);

      converter_.reset(new AudioConverter(input, output, false));
      converter_->AddInput(this);
      converter_->PrimeWithSilence();
      convert_bus_ = AudioBus::Create(output);

      // Update our packet size assumptions based on the new format.
      const auto new_bytes_per_buffer =
          static_cast<int>(new_frames_per_buffer) * input_format_.nBlockAlign;
      packet_size_frames_ = new_bytes_per_buffer / input_format_.nBlockAlign;
      packet_size_bytes_ = new_bytes_per_buffer;
      frame_size_ = input_format_.nBlockAlign;

      imperfect_buffer_size_conversion_ =
          std::modf(new_frames_per_buffer, &new_frames_per_buffer) != 0.0;
      DVLOG_IF(1, imperfect_buffer_size_conversion_)
          << "Audio capture data conversion: Need to inject fifo";

      // Indicate that we're good to go with a close match.
      hresult = S_OK;
    }
  }

  // At this point, |hresult| == S_OK if the desired format is supported. If
  // |hresult| == S_FALSE, the OS supports a closest match but we don't support
  // conversion to it. Thus, SUCCEEDED() or FAILED() can't be used to determine
  // if the desired format is supported.
  *hr = hresult;
  return (hresult == S_OK);
}

HRESULT WASAPIAudioInputStream::InitializeAudioEngine() {
  DCHECK_EQ(OPEN_RESULT_OK, open_result_);
  DWORD flags;
  // Use event-driven mode only fo regular input devices. For loopback the
  // EVENTCALLBACK flag is specified when intializing
  // |audio_render_client_for_loopback_|.
  if (device_id_ == AudioDeviceDescription::kLoopbackInputDeviceId ||
      device_id_ == AudioDeviceDescription::kLoopbackWithMuteDeviceId) {
    flags = AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_NOPERSIST;
  } else {
    flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST;
  }

  // Initialize the audio stream between the client and the device.
  // We connect indirectly through the audio engine by using shared mode.
  // Note that, |hnsBufferDuration| is set of 0, which ensures that the
  // buffer is never smaller than the minimum buffer size needed to ensure
  // that glitches do not occur between the periodic processing passes.
  // This setting should lead to lowest possible latency.
  HRESULT hr = audio_client_->Initialize(
      AUDCLNT_SHAREMODE_SHARED, flags,
      0,  // hnsBufferDuration
      0, &input_format_,
      device_id_ == AudioDeviceDescription::kCommunicationsDeviceId
          ? &kCommunicationsSessionId
          : nullptr);

  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_AUDIO_CLIENT_INIT_FAILED;
    base::UmaHistogramSparse("Media.Audio.Capture.Win.InitError", hr);
    return hr;
  }

  // Retrieve the length of the endpoint buffer shared between the client
  // and the audio engine. The buffer length determines the maximum amount
  // of capture data that the audio engine can read from the endpoint buffer
  // during a single processing pass.
  // A typical value is 960 audio frames <=> 20ms @ 48kHz sample rate.
  hr = audio_client_->GetBufferSize(&endpoint_buffer_size_frames_);
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_GET_BUFFER_SIZE_FAILED;
    return hr;
  }

  DVLOG(1) << "endpoint buffer size: " << endpoint_buffer_size_frames_
           << " [frames]";

#ifndef NDEBUG
  // The period between processing passes by the audio engine is fixed for a
  // particular audio endpoint device and represents the smallest processing
  // quantum for the audio engine. This period plus the stream latency between
  // the buffer and endpoint device represents the minimum possible latency
  // that an audio application can achieve.
  // TODO(henrika): possibly remove this section when all parts are ready.
  REFERENCE_TIME device_period_shared_mode = 0;
  REFERENCE_TIME device_period_exclusive_mode = 0;
  HRESULT hr_dbg = audio_client_->GetDevicePeriod(
      &device_period_shared_mode, &device_period_exclusive_mode);
  if (SUCCEEDED(hr_dbg)) {
    DVLOG(1) << "device period: "
             << static_cast<double>(device_period_shared_mode / 10000.0)
             << " [ms]";
  }

  REFERENCE_TIME latency = 0;
  hr_dbg = audio_client_->GetStreamLatency(&latency);
  if (SUCCEEDED(hr_dbg)) {
    DVLOG(1) << "stream latency: " << static_cast<double>(latency / 10000.0)
             << " [ms]";
  }
#endif

  // Set the event handle that the audio engine will signal each time a buffer
  // becomes ready to be processed by the client.
  //
  // In loopback case the capture device doesn't receive any events, so we
  // need to create a separate playback client to get notifications. According
  // to MSDN:
  //
  //   A pull-mode capture client does not receive any events when a stream is
  //   initialized with event-driven buffering and is loopback-enabled. To
  //   work around this, initialize a render stream in event-driven mode. Each
  //   time the client receives an event for the render stream, it must signal
  //   the capture client to run the capture thread that reads the next set of
  //   samples from the capture endpoint buffer.
  //
  // http://msdn.microsoft.com/en-us/library/windows/desktop/dd316551(v=vs.85).aspx
  if (device_id_ == AudioDeviceDescription::kLoopbackInputDeviceId ||
      device_id_ == AudioDeviceDescription::kLoopbackWithMuteDeviceId) {
    hr = endpoint_device_->Activate(
        __uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL,
        &audio_render_client_for_loopback_);
    if (FAILED(hr)) {
      open_result_ = OPEN_RESULT_LOOPBACK_ACTIVATE_FAILED;
      return hr;
    }

    hr = audio_render_client_for_loopback_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST, 0, 0,
        &input_format_, NULL);
    if (FAILED(hr)) {
      open_result_ = OPEN_RESULT_LOOPBACK_INIT_FAILED;
      return hr;
    }

    hr = audio_render_client_for_loopback_->SetEventHandle(
        audio_samples_ready_event_.Get());
  } else {
    hr = audio_client_->SetEventHandle(audio_samples_ready_event_.Get());
  }

  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_SET_EVENT_HANDLE;
    return hr;
  }

  // Get access to the IAudioCaptureClient interface. This interface
  // enables us to read input data from the capture endpoint buffer.
  hr = audio_client_->GetService(IID_PPV_ARGS(&audio_capture_client_));
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_NO_CAPTURE_CLIENT;
    return hr;
  }

  // Obtain a reference to the ISimpleAudioVolume interface which enables
  // us to control the master volume level of an audio session.
  hr = audio_client_->GetService(IID_PPV_ARGS(&simple_audio_volume_));
  if (FAILED(hr))
    open_result_ = OPEN_RESULT_NO_AUDIO_VOLUME;

  return hr;
}

void WASAPIAudioInputStream::ReportOpenResult(HRESULT hr) const {
  DCHECK(!opened_);  // This method must be called before we set this flag.
  UMA_HISTOGRAM_ENUMERATION("Media.Audio.Capture.Win.Open", open_result_,
                            OPEN_RESULT_MAX + 1);
  if (open_result_ != OPEN_RESULT_OK &&
      open_result_ != OPEN_RESULT_OK_WITH_RESAMPLING) {
    log_callback_.Run(base::StringPrintf(
        "WASAPIAIS::Open: failed, result = %d, hresult = %#lx, "
        "input format = %#x/%d/%ld/%d/%d/%ld/%d, "
        "output format = %#x/%d/%ld/%d/%d/%ld/%d",
        // clang-format off
        open_result_, hr,
        input_format_.wFormatTag, input_format_.nChannels,
        input_format_.nSamplesPerSec, input_format_.wBitsPerSample,
        input_format_.nBlockAlign, input_format_.nAvgBytesPerSec,
        input_format_.cbSize,
        output_format_.wFormatTag, output_format_.nChannels,
        output_format_.nSamplesPerSec, output_format_.wBitsPerSample,
        output_format_.nBlockAlign,output_format_.nAvgBytesPerSec,
        output_format_.cbSize));
    // clang-format on
  }
}

double WASAPIAudioInputStream::ProvideInput(AudioBus* audio_bus,
                                            uint32_t frames_delayed) {
  fifo_->Consume()->CopyTo(audio_bus);
  return 1.0;
}

}  // namespace media
