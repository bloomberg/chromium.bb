// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/core_audio_util_win.h"

#include <Audioclient.h>
#include <Functiondiscoverykeys_devpkey.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/windows_version.h"
#include "media/base/media_switches.h"

using base::win::ScopedCoMem;
using base::win::ScopedHandle;

namespace media {

typedef uint32 ChannelConfig;

// Converts Microsoft's channel configuration to ChannelLayout.
// This mapping is not perfect but the best we can do given the current
// ChannelLayout enumerator and the Windows-specific speaker configurations
// defined in ksmedia.h. Don't assume that the channel ordering in
// ChannelLayout is exactly the same as the Windows specific configuration.
// As an example: KSAUDIO_SPEAKER_7POINT1_SURROUND is mapped to
// CHANNEL_LAYOUT_7_1 but the positions of Back L, Back R and Side L, Side R
// speakers are different in these two definitions.
static ChannelLayout ChannelConfigToChannelLayout(ChannelConfig config) {
  switch (config) {
    case KSAUDIO_SPEAKER_DIRECTOUT:
      DVLOG(2) << "KSAUDIO_SPEAKER_DIRECTOUT=>CHANNEL_LAYOUT_NONE";
      return CHANNEL_LAYOUT_NONE;
    case KSAUDIO_SPEAKER_MONO:
      DVLOG(2) << "KSAUDIO_SPEAKER_MONO=>CHANNEL_LAYOUT_MONO";
      return CHANNEL_LAYOUT_MONO;
    case KSAUDIO_SPEAKER_STEREO:
      DVLOG(2) << "KSAUDIO_SPEAKER_STEREO=>CHANNEL_LAYOUT_STEREO";
      return CHANNEL_LAYOUT_STEREO;
    case KSAUDIO_SPEAKER_QUAD:
      DVLOG(2) << "KSAUDIO_SPEAKER_QUAD=>CHANNEL_LAYOUT_QUAD";
      return CHANNEL_LAYOUT_QUAD;
    case KSAUDIO_SPEAKER_SURROUND:
      DVLOG(2) << "KSAUDIO_SPEAKER_SURROUND=>CHANNEL_LAYOUT_4_0";
      return CHANNEL_LAYOUT_4_0;
    case KSAUDIO_SPEAKER_5POINT1:
      DVLOG(2) << "KSAUDIO_SPEAKER_5POINT1=>CHANNEL_LAYOUT_5_1_BACK";
      return CHANNEL_LAYOUT_5_1_BACK;
    case KSAUDIO_SPEAKER_5POINT1_SURROUND:
      DVLOG(2) << "KSAUDIO_SPEAKER_5POINT1_SURROUND=>CHANNEL_LAYOUT_5_1";
      return CHANNEL_LAYOUT_5_1;
    case KSAUDIO_SPEAKER_7POINT1:
      DVLOG(2) << "KSAUDIO_SPEAKER_7POINT1=>CHANNEL_LAYOUT_7_1_WIDE";
      return CHANNEL_LAYOUT_7_1_WIDE;
    case KSAUDIO_SPEAKER_7POINT1_SURROUND:
      DVLOG(2) << "KSAUDIO_SPEAKER_7POINT1_SURROUND=>CHANNEL_LAYOUT_7_1";
      return CHANNEL_LAYOUT_7_1;
    default:
      DVLOG(2) << "Unsupported channel layout: " << config;
      return CHANNEL_LAYOUT_UNSUPPORTED;
  }
}

bool LoadAudiosesDll() {
  static const wchar_t* const kAudiosesDLL =
      L"%WINDIR%\\system32\\audioses.dll";

  wchar_t path[MAX_PATH] = {0};
  ExpandEnvironmentStringsW(kAudiosesDLL, path, arraysize(path));
  return (LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH) != NULL);
}

bool CanCreateDeviceEnumerator() {
  ScopedComPtr<IMMDeviceEnumerator> device_enumerator;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                NULL,
                                CLSCTX_INPROC_SERVER,
                                __uuidof(IMMDeviceEnumerator),
                                device_enumerator.ReceiveVoid());

  // If we hit CO_E_NOTINITIALIZED, CoInitialize has not been called and it
  // must be called at least once for each thread that uses the COM library.
  CHECK_NE(hr, CO_E_NOTINITIALIZED);

  return SUCCEEDED(hr);
}

bool CoreAudioUtil::IsSupported() {
  // Microsoft does not plan to make the Core Audio APIs available for use
  // with earlier versions of Windows, including Microsoft Windows Server 2003,
  // Windows XP, Windows Millennium Edition, Windows 2000, and Windows 98.
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return false;

  // The audio core APIs are implemented in the Mmdevapi.dll and Audioses.dll
  // system components.
  // Dependency Walker shows that it is enough to verify possibility to load
  // the Audioses DLL since it depends on Mmdevapi.dll.
  // See http://crbug.com/166397 why this extra step is required to guarantee
  // Core Audio support.
  static bool g_audioses_dll_available = LoadAudiosesDll();
  if (!g_audioses_dll_available)
    return false;

  // Being able to load the Audioses.dll does not seem to be sufficient for
  // all devices to guarantee Core Audio support. To be 100%, we also verify
  // that it is possible to a create the IMMDeviceEnumerator interface. If this
  // works as well we should be home free.
  static bool g_can_create_device_enumerator = CanCreateDeviceEnumerator();
  LOG_IF(ERROR, !g_can_create_device_enumerator)
      << "Failed to create Core Audio device enumerator on thread with ID "
      << GetCurrentThreadId();
  return g_can_create_device_enumerator;
}

base::TimeDelta CoreAudioUtil::RefererenceTimeToTimeDelta(REFERENCE_TIME time) {
  // Each unit of reference time is 100 nanoseconds <=> 0.1 microsecond.
  return base::TimeDelta::FromMicroseconds(0.1 * time + 0.5);
}

AUDCLNT_SHAREMODE CoreAudioUtil::GetShareMode() {
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableExclusiveAudio))
    return AUDCLNT_SHAREMODE_EXCLUSIVE;
  return AUDCLNT_SHAREMODE_SHARED;
}

int CoreAudioUtil::NumberOfActiveDevices(EDataFlow data_flow) {
  DCHECK(IsSupported());
  // Create the IMMDeviceEnumerator interface.
  ScopedComPtr<IMMDeviceEnumerator> device_enumerator =
      CreateDeviceEnumerator();
  if (!device_enumerator)
    return 0;

  // Generate a collection of active (present and not disabled) audio endpoint
  // devices for the specified data-flow direction.
  // This method will succeed even if all devices are disabled.
  ScopedComPtr<IMMDeviceCollection> collection;
  HRESULT hr = device_enumerator->EnumAudioEndpoints(data_flow,
                                                     DEVICE_STATE_ACTIVE,
                                                     collection.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "IMMDeviceCollection::EnumAudioEndpoints: " << std::hex << hr;
    return 0;
  }

  // Retrieve the number of active audio devices for the specified direction
  UINT number_of_active_devices = 0;
  collection->GetCount(&number_of_active_devices);
  DVLOG(2) << ((data_flow == eCapture) ? "[in ] " : "[out] ")
           << "number of devices: " << number_of_active_devices;
  return static_cast<int>(number_of_active_devices);
}

ScopedComPtr<IMMDeviceEnumerator> CoreAudioUtil::CreateDeviceEnumerator() {
  DCHECK(IsSupported());
  ScopedComPtr<IMMDeviceEnumerator> device_enumerator;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                NULL,
                                CLSCTX_INPROC_SERVER,
                                __uuidof(IMMDeviceEnumerator),
                                device_enumerator.ReceiveVoid());
  LOG_IF(ERROR, FAILED(hr)) << "IMMDeviceEnumerator::CreateDeviceEnumerator: "
                            << std::hex << hr;
  return device_enumerator;
}

ScopedComPtr<IMMDevice> CoreAudioUtil::CreateDefaultDevice(EDataFlow data_flow,
                                                           ERole role) {
  DCHECK(IsSupported());
  ScopedComPtr<IMMDevice> endpoint_device;

  // Create the IMMDeviceEnumerator interface.
  ScopedComPtr<IMMDeviceEnumerator> device_enumerator =
      CreateDeviceEnumerator();
  if (!device_enumerator)
    return endpoint_device;

  // Retrieve the default audio endpoint for the specified data-flow
  // direction and role.
  HRESULT hr = device_enumerator->GetDefaultAudioEndpoint(
      data_flow, role, endpoint_device.Receive());

  if (FAILED(hr)) {
    DVLOG(1) << "IMMDeviceEnumerator::GetDefaultAudioEndpoint: "
             << std::hex << hr;
    return endpoint_device;
  }

  // Verify that the audio endpoint device is active, i.e., that the audio
  // adapter that connects to the endpoint device is present and enabled.
  DWORD state = DEVICE_STATE_DISABLED;
  hr = endpoint_device->GetState(&state);
  if (SUCCEEDED(hr)) {
    if (!(state & DEVICE_STATE_ACTIVE)) {
      DVLOG(1) << "Selected endpoint device is not active";
      endpoint_device.Release();
    }
  }
  return endpoint_device;
}

ScopedComPtr<IMMDevice> CoreAudioUtil::CreateDevice(
    const std::string& device_id) {
  DCHECK(IsSupported());
  ScopedComPtr<IMMDevice> endpoint_device;

  // Create the IMMDeviceEnumerator interface.
  ScopedComPtr<IMMDeviceEnumerator> device_enumerator =
      CreateDeviceEnumerator();
  if (!device_enumerator)
    return endpoint_device;

  // Retrieve an audio device specified by an endpoint device-identification
  // string.
  HRESULT hr = device_enumerator->GetDevice(UTF8ToUTF16(device_id).c_str(),
                                            endpoint_device.Receive());
  DVLOG_IF(1, FAILED(hr)) << "IMMDeviceEnumerator::GetDevice: "
                          << std::hex << hr;
  return endpoint_device;
}

HRESULT CoreAudioUtil::GetDeviceName(IMMDevice* device, AudioDeviceName* name) {
  DCHECK(IsSupported());

  // Retrieve unique name of endpoint device.
  // Example: "{0.0.1.00000000}.{8db6020f-18e3-4f25-b6f5-7726c9122574}".
  AudioDeviceName device_name;
  ScopedCoMem<WCHAR> endpoint_device_id;
  HRESULT hr = device->GetId(&endpoint_device_id);
  if (FAILED(hr))
    return hr;
  WideToUTF8(endpoint_device_id, wcslen(endpoint_device_id),
             &device_name.unique_id);

  // Retrieve user-friendly name of endpoint device.
  // Example: "Microphone (Realtek High Definition Audio)".
  ScopedComPtr<IPropertyStore> properties;
  hr = device->OpenPropertyStore(STGM_READ, properties.Receive());
  if (FAILED(hr))
    return hr;
  base::win::ScopedPropVariant friendly_name;
  hr = properties->GetValue(PKEY_Device_FriendlyName, friendly_name.Receive());
  if (FAILED(hr))
    return hr;
  if (friendly_name.get().vt == VT_LPWSTR && friendly_name.get().pwszVal) {
    WideToUTF8(friendly_name.get().pwszVal,
               wcslen(friendly_name.get().pwszVal),
               &device_name.device_name);
  }

  *name = device_name;
  DVLOG(2) << "friendly name: " << device_name.device_name;
  DVLOG(2) << "unique id    : " << device_name.unique_id;
  return hr;
}

std::string CoreAudioUtil::GetFriendlyName(const std::string& device_id) {
  DCHECK(IsSupported());
  ScopedComPtr<IMMDevice> audio_device = CreateDevice(device_id);
  if (!audio_device)
    return std::string();

  AudioDeviceName device_name;
  HRESULT hr = GetDeviceName(audio_device, &device_name);
  if (FAILED(hr))
    return std::string();

  return device_name.device_name;
}

bool CoreAudioUtil::DeviceIsDefault(EDataFlow flow,
                                    ERole role,
                                    std::string device_id) {
  DCHECK(IsSupported());
  ScopedComPtr<IMMDevice> device = CreateDefaultDevice(flow, role);
  if (!device)
    return false;

  ScopedCoMem<WCHAR> default_device_id;
  HRESULT hr = device->GetId(&default_device_id);
  if (FAILED(hr))
    return false;

  std::string str_default;
  WideToUTF8(default_device_id, wcslen(default_device_id), &str_default);
  if (device_id.compare(str_default) != 0)
    return false;
  return true;
}

EDataFlow CoreAudioUtil::GetDataFlow(IMMDevice* device) {
  DCHECK(IsSupported());
  ScopedComPtr<IMMEndpoint> endpoint;
  HRESULT hr = device->QueryInterface(endpoint.Receive());
  if (FAILED(hr)) {
    DVLOG(1) << "IMMDevice::QueryInterface: " << std::hex << hr;
    return eAll;
  }

  EDataFlow data_flow;
  hr = endpoint->GetDataFlow(&data_flow);
  if (FAILED(hr)) {
    DVLOG(1) << "IMMEndpoint::GetDataFlow: " << std::hex << hr;
    return eAll;
  }
  return data_flow;
}

ScopedComPtr<IAudioClient> CoreAudioUtil::CreateClient(
    IMMDevice* audio_device) {
  DCHECK(IsSupported());

  // Creates and activates an IAudioClient COM object given the selected
  // endpoint device.
  ScopedComPtr<IAudioClient> audio_client;
  HRESULT hr = audio_device->Activate(__uuidof(IAudioClient),
                                      CLSCTX_INPROC_SERVER,
                                      NULL,
                                      audio_client.ReceiveVoid());
  DVLOG_IF(1, FAILED(hr)) << "IMMDevice::Activate: " << std::hex << hr;
  return audio_client;
}

ScopedComPtr<IAudioClient> CoreAudioUtil::CreateDefaultClient(
    EDataFlow data_flow, ERole role) {
  DCHECK(IsSupported());
  ScopedComPtr<IMMDevice> default_device(CreateDefaultDevice(data_flow, role));
  return (default_device ? CreateClient(default_device) :
      ScopedComPtr<IAudioClient>());
}

HRESULT CoreAudioUtil::GetSharedModeMixFormat(
    IAudioClient* client, WAVEFORMATPCMEX* format) {
  DCHECK(IsSupported());
  ScopedCoMem<WAVEFORMATPCMEX> format_pcmex;
  HRESULT hr = client->GetMixFormat(
      reinterpret_cast<WAVEFORMATEX**>(&format_pcmex));
  if (FAILED(hr))
    return hr;

  size_t bytes = sizeof(WAVEFORMATEX) + format_pcmex->Format.cbSize;
  DCHECK_EQ(bytes, sizeof(WAVEFORMATPCMEX));

  memcpy(format, format_pcmex, bytes);

  DVLOG(2) << "wFormatTag: 0x" << std::hex << format->Format.wFormatTag
           << ", nChannels: " << std::dec << format->Format.nChannels
           << ", nSamplesPerSec: " << format->Format.nSamplesPerSec
           << ", nAvgBytesPerSec: " << format->Format.nAvgBytesPerSec
           << ", nBlockAlign: " << format->Format.nBlockAlign
           << ", wBitsPerSample: " << format->Format.wBitsPerSample
           << ", cbSize: " << format->Format.cbSize
           << ", wValidBitsPerSample: " << format->Samples.wValidBitsPerSample
           << ", dwChannelMask: 0x" << std::hex << format->dwChannelMask;

  return hr;
}

HRESULT CoreAudioUtil::GetDefaultSharedModeMixFormat(
    EDataFlow data_flow, ERole role, WAVEFORMATPCMEX* format) {
  DCHECK(IsSupported());
  ScopedComPtr<IAudioClient> client(CreateDefaultClient(data_flow, role));
  if (!client) {
    // Map NULL-pointer to new error code which can be different from the
    // actual error code. The exact value is not important here.
    return AUDCLNT_E_ENDPOINT_CREATE_FAILED;
  }
  return CoreAudioUtil::GetSharedModeMixFormat(client, format);
}

bool CoreAudioUtil::IsFormatSupported(IAudioClient* client,
                                      AUDCLNT_SHAREMODE share_mode,
                                      const WAVEFORMATPCMEX* format) {
  DCHECK(IsSupported());
  ScopedCoMem<WAVEFORMATEXTENSIBLE> closest_match;
  HRESULT hr = client->IsFormatSupported(
      share_mode, reinterpret_cast<const WAVEFORMATEX*>(format),
      reinterpret_cast<WAVEFORMATEX**>(&closest_match));

  // This log can only be triggered for shared mode.
  DLOG_IF(ERROR, hr == S_FALSE) << "Format is not supported "
                                << "but a closest match exists.";
  // This log can be triggered both for shared and exclusive modes.
  DLOG_IF(ERROR, hr == AUDCLNT_E_UNSUPPORTED_FORMAT) << "Unsupported format.";
  if (hr == S_FALSE) {
    DVLOG(2) << "wFormatTag: " << closest_match->Format.wFormatTag
             << ", nChannels: " << closest_match->Format.nChannels
             << ", nSamplesPerSec: " << closest_match->Format.nSamplesPerSec
             << ", wBitsPerSample: " << closest_match->Format.wBitsPerSample;
  }

  return (hr == S_OK);
}

HRESULT CoreAudioUtil::GetDevicePeriod(IAudioClient* client,
                                       AUDCLNT_SHAREMODE share_mode,
                                       REFERENCE_TIME* device_period) {
  DCHECK(IsSupported());

  // Get the period of the engine thread.
  REFERENCE_TIME default_period = 0;
  REFERENCE_TIME minimum_period = 0;
  HRESULT hr = client->GetDevicePeriod(&default_period, &minimum_period);
  if (FAILED(hr))
    return hr;

  *device_period = (share_mode == AUDCLNT_SHAREMODE_SHARED) ? default_period :
      minimum_period;
  DVLOG(2) << "device_period: "
           << RefererenceTimeToTimeDelta(*device_period).InMillisecondsF()
           << " [ms]";
  return hr;
}

HRESULT CoreAudioUtil::GetPreferredAudioParameters(
    IAudioClient* client, AudioParameters* params) {
  DCHECK(IsSupported());
  WAVEFORMATPCMEX mix_format;
  HRESULT hr = GetSharedModeMixFormat(client, &mix_format);
  if (FAILED(hr))
    return hr;

  REFERENCE_TIME default_period = 0;
  hr = GetDevicePeriod(client, AUDCLNT_SHAREMODE_SHARED, &default_period);
  if (FAILED(hr))
    return hr;

  // Get the integer mask which corresponds to the channel layout the
  // audio engine uses for its internal processing/mixing of shared-mode
  // streams. This mask indicates which channels are present in the multi-
  // channel stream. The least significant bit corresponds with the Front Left
  // speaker, the next least significant bit corresponds to the Front Right
  // speaker, and so on, continuing in the order defined in KsMedia.h.
  // See http://msdn.microsoft.com/en-us/library/windows/hardware/ff537083.aspx
  // for more details.
  ChannelConfig channel_config = mix_format.dwChannelMask;

  // Convert Microsoft's channel configuration to genric ChannelLayout.
  ChannelLayout channel_layout = ChannelConfigToChannelLayout(channel_config);

  // Preferred sample rate.
  int sample_rate = mix_format.Format.nSamplesPerSec;

  // TODO(henrika): possibly use format.Format.wBitsPerSample here instead.
  // We use a hard-coded value of 16 bits per sample today even if most audio
  // engines does the actual mixing in 32 bits per sample.
  int bits_per_sample = 16;

  // We are using the native device period to derive the smallest possible
  // buffer size in shared mode. Note that the actual endpoint buffer will be
  // larger than this size but it will be possible to fill it up in two calls.
  // TODO(henrika): ensure that this scheme works for capturing as well.
  int frames_per_buffer = static_cast<int>(sample_rate *
      RefererenceTimeToTimeDelta(default_period).InSecondsF() + 0.5);

  DVLOG(1) << "channel_layout   : " << channel_layout;
  DVLOG(1) << "sample_rate      : " << sample_rate;
  DVLOG(1) << "bits_per_sample  : " << bits_per_sample;
  DVLOG(1) << "frames_per_buffer: " << frames_per_buffer;

  AudioParameters audio_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               channel_layout,
                               sample_rate,
                               bits_per_sample,
                               frames_per_buffer);

  *params = audio_params;
  return hr;
}

HRESULT CoreAudioUtil::GetPreferredAudioParameters(
    EDataFlow data_flow, ERole role, AudioParameters* params) {
  DCHECK(IsSupported());
  ScopedComPtr<IAudioClient> client(CreateDefaultClient(data_flow, role));
  if (!client) {
    // Map NULL-pointer to new error code which can be different from the
    // actual error code. The exact value is not important here.
    return AUDCLNT_E_ENDPOINT_CREATE_FAILED;
  }
  return GetPreferredAudioParameters(client, params);
}

HRESULT CoreAudioUtil::SharedModeInitialize(IAudioClient* client,
                                            const WAVEFORMATPCMEX* format,
                                            HANDLE event_handle,
                                            uint32* endpoint_buffer_size) {
  DCHECK(IsSupported());

  // Use default flags (i.e, dont set AUDCLNT_STREAMFLAGS_NOPERSIST) to
  // ensure that the volume level and muting state for a rendering session
  // are persistent across system restarts. The volume level and muting
  // state for a capture session are never persistent.
  DWORD stream_flags = 0;

  // Enable event-driven streaming if a valid event handle is provided.
  // After the stream starts, the audio engine will signal the event handle
  // to notify the client each time a buffer becomes ready to process.
  // Event-driven buffering is supported for both rendering and capturing.
  // Both shared-mode and exclusive-mode streams can use event-driven buffering.
  bool use_event = (event_handle != NULL &&
                    event_handle != INVALID_HANDLE_VALUE);
  if (use_event)
    stream_flags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
  DVLOG(2) << "stream_flags: 0x" << std::hex << stream_flags;

  // Initialize the shared mode client for minimal delay.
  HRESULT hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                  stream_flags,
                                  0,
                                  0,
                                  reinterpret_cast<const WAVEFORMATEX*>(format),
                                  NULL);
  if (FAILED(hr)) {
    DVLOG(1) << "IAudioClient::Initialize: " << std::hex << hr;
    return hr;
  }

  if (use_event) {
    hr = client->SetEventHandle(event_handle);
    if (FAILED(hr)) {
      DVLOG(1) << "IAudioClient::SetEventHandle: " << std::hex << hr;
      return hr;
    }
  }

  UINT32 buffer_size_in_frames = 0;
  hr = client->GetBufferSize(&buffer_size_in_frames);
  if (FAILED(hr)) {
    DVLOG(1) << "IAudioClient::GetBufferSize: " << std::hex << hr;
    return hr;
  }

  *endpoint_buffer_size = buffer_size_in_frames;
  DVLOG(2) << "endpoint buffer size: " << buffer_size_in_frames;

  // TODO(henrika): utilize when delay measurements are added.
  REFERENCE_TIME  latency = 0;
  hr = client->GetStreamLatency(&latency);
  DVLOG(2) << "stream latency: "
           << RefererenceTimeToTimeDelta(latency).InMillisecondsF() << " [ms]";
  return hr;
}

ScopedComPtr<IAudioRenderClient> CoreAudioUtil::CreateRenderClient(
    IAudioClient* client) {
  DCHECK(IsSupported());

  // Get access to the IAudioRenderClient interface. This interface
  // enables us to write output data to a rendering endpoint buffer.
  ScopedComPtr<IAudioRenderClient> audio_render_client;
  HRESULT hr = client->GetService(__uuidof(IAudioRenderClient),
                                  audio_render_client.ReceiveVoid());
  if (FAILED(hr)) {
    DVLOG(1) << "IAudioClient::GetService: " << std::hex << hr;
    return ScopedComPtr<IAudioRenderClient>();
  }
  return audio_render_client;
}

ScopedComPtr<IAudioCaptureClient> CoreAudioUtil::CreateCaptureClient(
    IAudioClient* client) {
  DCHECK(IsSupported());

  // Get access to the IAudioCaptureClient interface. This interface
  // enables us to read input data from a capturing endpoint buffer.
  ScopedComPtr<IAudioCaptureClient> audio_capture_client;
  HRESULT hr = client->GetService(__uuidof(IAudioCaptureClient),
                                  audio_capture_client.ReceiveVoid());
  if (FAILED(hr)) {
    DVLOG(1) << "IAudioClient::GetService: " << std::hex << hr;
    return ScopedComPtr<IAudioCaptureClient>();
  }
  return audio_capture_client;
}

bool CoreAudioUtil::FillRenderEndpointBufferWithSilence(
    IAudioClient* client, IAudioRenderClient* render_client) {
  DCHECK(IsSupported());

  UINT32 endpoint_buffer_size = 0;
  if (FAILED(client->GetBufferSize(&endpoint_buffer_size)))
    return false;

  UINT32 num_queued_frames = 0;
  if (FAILED(client->GetCurrentPadding(&num_queued_frames)))
    return false;

  BYTE* data = NULL;
  int num_frames_to_fill = endpoint_buffer_size - num_queued_frames;
  if (FAILED(render_client->GetBuffer(num_frames_to_fill, &data)))
    return false;

  // Using the AUDCLNT_BUFFERFLAGS_SILENT flag eliminates the need to
  // explicitly write silence data to the rendering buffer.
  DVLOG(2) << "filling up " << num_frames_to_fill << " frames with silence";
  return SUCCEEDED(render_client->ReleaseBuffer(num_frames_to_fill,
                                                AUDCLNT_BUFFERFLAGS_SILENT));
}

}  // namespace media
