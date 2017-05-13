// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_manager_win.h"

#include <windows.h>
#include <objbase.h>  // This has to be before initguid.h
#include <initguid.h>
#include <mmsystem.h>
#include <setupapi.h>
#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/win/windows_version.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_io.h"
#include "media/audio/win/audio_device_listener_win.h"
#include "media/audio/win/audio_low_latency_input_win.h"
#include "media/audio/win/audio_low_latency_output_win.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/audio/win/device_enumeration_win.h"
#include "media/audio/win/waveout_output_win.h"
#include "media/base/audio_parameters.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

// The following are defined in various DDK headers, and we (re)define them here
// to avoid adding the DDK as a chrome dependency.
#define DRV_QUERYDEVICEINTERFACE 0x80c
#define DRVM_MAPPER_PREFERRED_GET 0x2015
#define DRV_QUERYDEVICEINTERFACESIZE 0x80d
DEFINE_GUID(AM_KSCATEGORY_AUDIO,
            0x6994ad04,
            0x93ef,
            0x11d0,
            0xa3,
            0xcc,
            0x00,
            0xa0,
            0xc9,
            0x22,
            0x31,
            0x96);

namespace media {

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

// Up to 8 channels can be passed to the driver.  This should work, given the
// right drivers, but graceful error handling is needed.
static const int kWinMaxChannels = 8;

// Buffer size to use for input and output stream when a proper size can't be
// determined from the system
static const int kFallbackBufferSize = 2048;

static int GetVersionPartAsInt(DWORDLONG num) {
  return static_cast<int>(num & 0xffff);
}

// Returns a string containing the given device's description and installed
// driver version.
static base::string16 GetDeviceAndDriverInfo(HDEVINFO device_info,
                                             SP_DEVINFO_DATA* device_data) {
  // Save the old install params setting and set a flag for the
  // SetupDiBuildDriverInfoList below to return only the installed drivers.
  SP_DEVINSTALL_PARAMS old_device_install_params;
  old_device_install_params.cbSize = sizeof(old_device_install_params);
  SetupDiGetDeviceInstallParams(device_info, device_data,
                                &old_device_install_params);
  SP_DEVINSTALL_PARAMS device_install_params = old_device_install_params;
  device_install_params.FlagsEx |= DI_FLAGSEX_INSTALLEDDRIVER;
  SetupDiSetDeviceInstallParams(device_info, device_data,
                                &device_install_params);

  SP_DRVINFO_DATA driver_data;
  driver_data.cbSize = sizeof(driver_data);
  base::string16 device_and_driver_info;
  if (SetupDiBuildDriverInfoList(device_info, device_data,
                                 SPDIT_COMPATDRIVER)) {
    if (SetupDiEnumDriverInfo(device_info, device_data, SPDIT_COMPATDRIVER, 0,
                              &driver_data)) {
      DWORDLONG version = driver_data.DriverVersion;
      device_and_driver_info =
          base::string16(driver_data.Description) + L" v" +
          base::IntToString16(GetVersionPartAsInt((version >> 48))) + L"." +
          base::IntToString16(GetVersionPartAsInt((version >> 32))) + L"." +
          base::IntToString16(GetVersionPartAsInt((version >> 16))) + L"." +
          base::IntToString16(GetVersionPartAsInt(version));
    }
    SetupDiDestroyDriverInfoList(device_info, device_data, SPDIT_COMPATDRIVER);
  }

  SetupDiSetDeviceInstallParams(device_info, device_data,
                                &old_device_install_params);

  return device_and_driver_info;
}

static int NumberOfWaveOutBuffers() {
  // Use the user provided buffer count if provided.
  int buffers = 0;
  std::string buffers_str(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWaveOutBuffers));
  if (base::StringToInt(buffers_str, &buffers) && buffers > 0) {
    return buffers;
  }

  // Use 4 buffers for Vista, 3 for everyone else:
  //  - The entire Windows audio stack was rewritten for Windows Vista and wave
  //    out performance was degraded compared to XP.
  //  - The regression was fixed in Windows 7 and most configurations will work
  //    with 2, but some (e.g., some Sound Blasters) still need 3.
  //  - Some XP configurations (even multi-processor ones) also need 3.
  return (base::win::GetVersion() == base::win::VERSION_VISTA) ? 4 : 3;
}

AudioManagerWin::AudioManagerWin(std::unique_ptr<AudioThread> audio_thread,
                                 AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {
  // |CoreAudioUtil::IsSupported()| uses static variables to avoid doing
  // multiple initializations.  This is however not thread safe.
  // So, here we call it explicitly before we kick off the audio thread
  // or do any other work.
  CoreAudioUtil::IsSupported();

  SetMaxOutputStreamsAllowed(kMaxOutputStreams);

  // WARNING: This is executed on the UI loop, do not add any code here which
  // loads libraries or attempts to call out into the OS.  Instead add such code
  // to the InitializeOnAudioThread() method below.

  // Task must be posted last to avoid races from handing out "this" to the
  // audio thread.
  GetTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&AudioManagerWin::InitializeOnAudioThread,
                            base::Unretained(this)));
}

AudioManagerWin::~AudioManagerWin() = default;

void AudioManagerWin::ShutdownOnAudioThread() {
  AudioManagerBase::ShutdownOnAudioThread();

  // Destroy AudioDeviceListenerWin instance on the audio thread because it
  // expects to be constructed and destroyed on the same thread.
  output_device_listener_.reset();
}

bool AudioManagerWin::HasAudioOutputDevices() {
  return (::waveOutGetNumDevs() != 0);
}

bool AudioManagerWin::HasAudioInputDevices() {
  return (::waveInGetNumDevs() != 0);
}

void AudioManagerWin::InitializeOnAudioThread() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // AudioDeviceListenerWin must be initialized on a COM thread.
  output_device_listener_.reset(new AudioDeviceListenerWin(BindToCurrentLoop(
      base::Bind(&AudioManagerWin::NotifyAllOutputDeviceChangeListeners,
                 base::Unretained(this)))));
}

base::string16 AudioManagerWin::GetAudioInputDeviceModel() {
  // Get the default audio capture device and its device interface name.
  DWORD device_id = 0;
  waveInMessage(reinterpret_cast<HWAVEIN>(WAVE_MAPPER),
                DRVM_MAPPER_PREFERRED_GET,
                reinterpret_cast<DWORD_PTR>(&device_id), NULL);
  ULONG device_interface_name_size = 0;
  waveInMessage(reinterpret_cast<HWAVEIN>(device_id),
                DRV_QUERYDEVICEINTERFACESIZE,
                reinterpret_cast<DWORD_PTR>(&device_interface_name_size), 0);
  size_t bytes_in_char16 = sizeof(base::string16::value_type);
  DCHECK_EQ(0u, device_interface_name_size % bytes_in_char16);
  if (device_interface_name_size <= bytes_in_char16)
    return base::string16();  // No audio capture device.

  base::string16 device_interface_name;
  base::string16::value_type* name_ptr = base::WriteInto(
      &device_interface_name, device_interface_name_size / bytes_in_char16);
  waveInMessage(reinterpret_cast<HWAVEIN>(device_id), DRV_QUERYDEVICEINTERFACE,
                reinterpret_cast<DWORD_PTR>(name_ptr),
                static_cast<DWORD_PTR>(device_interface_name_size));

  // Enumerate all audio devices and find the one matching the above device
  // interface name.
  HDEVINFO device_info = SetupDiGetClassDevs(
      &AM_KSCATEGORY_AUDIO, 0, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
  if (device_info == INVALID_HANDLE_VALUE)
    return base::string16();

  DWORD interface_index = 0;
  SP_DEVICE_INTERFACE_DATA interface_data;
  interface_data.cbSize = sizeof(interface_data);
  while (SetupDiEnumDeviceInterfaces(device_info, 0, &AM_KSCATEGORY_AUDIO,
                                     interface_index++, &interface_data)) {
    // Query the size of the struct, allocate it and then query the data.
    SP_DEVINFO_DATA device_data;
    device_data.cbSize = sizeof(device_data);
    DWORD interface_detail_size = 0;
    SetupDiGetDeviceInterfaceDetail(device_info, &interface_data, 0, 0,
                                    &interface_detail_size, &device_data);
    if (!interface_detail_size)
      continue;

    std::unique_ptr<char[]> interface_detail_buffer(
        new char[interface_detail_size]);
    SP_DEVICE_INTERFACE_DETAIL_DATA* interface_detail =
        reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(
            interface_detail_buffer.get());
    interface_detail->cbSize = interface_detail_size;
    if (!SetupDiGetDeviceInterfaceDetail(
            device_info, &interface_data, interface_detail,
            interface_detail_size, NULL, &device_data))
      return base::string16();

    bool device_found = (device_interface_name == interface_detail->DevicePath);

    if (device_found)
      return GetDeviceAndDriverInfo(device_info, &device_data);
  }

  return base::string16();
}

void AudioManagerWin::ShowAudioInputSettings() {
  base::FilePath path;
  PathService::Get(base::DIR_SYSTEM, &path);
  path = path.Append(L"control.exe");
  base::CommandLine command_line(path);
  command_line.AppendArg("mmsys.cpl,,1");
  base::LaunchProcess(command_line, base::LaunchOptions());
}

void AudioManagerWin::GetAudioDeviceNamesImpl(bool input,
                                              AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  // Enumerate all active audio-endpoint capture devices.
  if (input)
    GetInputDeviceNamesWin(device_names);
  else
    GetOutputDeviceNamesWin(device_names);

  if (!device_names->empty()) {
    device_names->push_front(AudioDeviceName::CreateCommunications());

    // Always add default device parameters as first element.
    device_names->push_front(AudioDeviceName::CreateDefault());
  }
}

void AudioManagerWin::GetAudioInputDeviceNames(AudioDeviceNames* device_names) {
  GetAudioDeviceNamesImpl(true, device_names);
}

void AudioManagerWin::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  GetAudioDeviceNamesImpl(false, device_names);
}

AudioParameters AudioManagerWin::GetInputStreamParameters(
    const std::string& device_id) {
  AudioParameters parameters;
  HRESULT hr =
      CoreAudioUtil::GetPreferredAudioParameters(device_id, false, &parameters);

  if (FAILED(hr) || !parameters.IsValid()) {
    LOG(WARNING) << "Unable to get preferred audio params for " << device_id
                 << " 0x" << std::hex << hr;
    // TODO(tommi): We appear to have callers to GetInputStreamParameters that
    // rely on getting valid audio parameters returned for an invalid or
    // unavailable device. We should track down those code paths (it is likely
    // that they actually don't need a real device but depend on the audio
    // code path somehow for a configuration - e.g. tab capture).
    parameters =
        AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                        CHANNEL_LAYOUT_STEREO, 48000, 16, kFallbackBufferSize);
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    parameters.set_frames_per_buffer(user_buffer_size);

  return parameters;
}

std::string AudioManagerWin::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  return CoreAudioUtil::GetMatchingOutputDeviceID(input_device_id);
}

const char* AudioManagerWin::GetName() {
  return "Windows";
}

// Factory for the implementations of AudioOutputStream for AUDIO_PCM_LINEAR
// mode.
// - PCMWaveOutAudioOutputStream: Based on the waveOut API.
AudioOutputStream* AudioManagerWin::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  if (params.channels() > kWinMaxChannels)
    return NULL;

  return new PCMWaveOutAudioOutputStream(this, params, NumberOfWaveOutBuffers(),
                                         WAVE_MAPPER);
}

// Factory for the implementations of AudioOutputStream for
// AUDIO_PCM_LOW_LATENCY mode. Two implementations should suffice most
// windows user's needs.
// - PCMWaveOutAudioOutputStream: Based on the waveOut API.
// - WASAPIAudioOutputStream: Based on Core Audio (WASAPI) API.
AudioOutputStream* AudioManagerWin::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  if (params.channels() > kWinMaxChannels)
    return NULL;

  // Pass an empty string to indicate that we want the default device
  // since we consistently only check for an empty string in
  // WASAPIAudioOutputStream.
  bool communications =
      device_id == AudioDeviceDescription::kCommunicationsDeviceId;
  return new WASAPIAudioOutputStream(
      this,
      communications || device_id == AudioDeviceDescription::kDefaultDeviceId
          ? std::string()
          : device_id,
      params, communications ? eCommunications : eConsole);
}

// Factory for the implementations of AudioInputStream for AUDIO_PCM_LINEAR
// mode.
AudioInputStream* AudioManagerWin::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeLowLatencyInputStream(params, device_id, log_callback);
}

// Factory for the implementations of AudioInputStream for
// AUDIO_PCM_LOW_LATENCY mode.
AudioInputStream* AudioManagerWin::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  // Used for both AUDIO_PCM_LOW_LATENCY and AUDIO_PCM_LINEAR.
  DVLOG(1) << "MakeLowLatencyInputStream: " << device_id;
  return new WASAPIAudioInputStream(this, params, device_id);
}

std::string AudioManagerWin::GetDefaultOutputDeviceID() {
  return CoreAudioUtil::GetDefaultOutputDeviceID();
}

AudioParameters AudioManagerWin::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  int sample_rate = 48000;
  int buffer_size = kFallbackBufferSize;
  int bits_per_sample = 16;
  int effects = AudioParameters::NO_EFFECTS;

  // TODO(henrika): Remove kEnableExclusiveAudio and related code. It doesn't
  // look like it's used.
  if (cmd_line->HasSwitch(switches::kEnableExclusiveAudio)) {
    // TODO(rtoy): tune these values for best possible WebAudio
    // performance. WebRTC works well at 48kHz and a buffer size of 480
    // samples will be used for this case. Note that exclusive mode is
    // experimental. This sample rate will be combined with a buffer size of
    // 256 samples, which corresponds to an output delay of ~5.33ms.
    sample_rate = 48000;
    buffer_size = 256;
    if (input_params.IsValid())
      channel_layout = input_params.channel_layout();
  } else {
    AudioParameters params;
    HRESULT hr = CoreAudioUtil::GetPreferredAudioParameters(
        output_device_id.empty() ? GetDefaultOutputDeviceID()
                                 : output_device_id,
        true, &params);
    if (FAILED(hr)) {
      // This can happen when CoreAudio isn't supported or available
      // (e.g. certain installations of Windows Server 2008 R2).
      // Instead of returning the input_params, we'll return invalid
      // AudioParameters to make sure that an attempt to create this output
      // stream, won't succeed. This behavior is also consistent with
      // GetInputStreamParameters.
      DLOG(ERROR) << "GetPreferredAudioParameters failed: " << std::hex << hr;
      return AudioParameters();
    }

    bits_per_sample = params.bits_per_sample();
    buffer_size = params.frames_per_buffer();
    channel_layout = params.channel_layout();
    sample_rate = params.sample_rate();
    effects = params.effects();
  }

  if (input_params.IsValid()) {
    // If the user has enabled checking supported channel layouts or we don't
    // have a valid channel layout yet, try to use the input layout.  See bugs
    // http://crbug.com/259165 and http://crbug.com/311906 for more details.
    if (cmd_line->HasSwitch(switches::kTrySupportedChannelLayouts) ||
        channel_layout == CHANNEL_LAYOUT_UNSUPPORTED) {
      // Check if it is possible to open up at the specified input channel
      // layout but avoid checking if the specified layout is the same as the
      // hardware (preferred) layout. We do this extra check to avoid the
      // CoreAudioUtil::IsChannelLayoutSupported() overhead in most cases.
      if (input_params.channel_layout() != channel_layout) {
        // TODO(henrika): Internally, IsChannelLayoutSupported does many of the
        // operations that have already been done such as opening up a client
        // and fetching the WAVEFORMATPCMEX format.  Ideally we should only do
        // that once.  Then here, we can check the layout from the data we
        // already hold.
        if (CoreAudioUtil::IsChannelLayoutSupported(
                output_device_id, eRender, eConsole,
                input_params.channel_layout())) {
          // Open up using the same channel layout as the source if it is
          // supported by the hardware.
          channel_layout = input_params.channel_layout();
          DVLOG(1) << "Hardware channel layout is not used; using same layout"
                   << " as the source instead (" << channel_layout << ")";
        }
      }
    }

    effects |= input_params.effects();
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
                         sample_rate, bits_per_sample, buffer_size);
  params.set_effects(effects);
  return params;
}

// static
std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return base::MakeUnique<AudioManagerWin>(std::move(audio_thread),
                                           audio_log_factory);
}

}  // namespace media
