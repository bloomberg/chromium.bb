// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Software adjust volume of samples, allows each audio stream its own
// volume without impacting master volume for chrome and other applications.

// Implemented as templates to allow 8, 16 and 32 bit implementations.
// 8 bit is unsigned and biased by 128.

// TODO(vrk): This file has been running pretty wild and free, and it's likely
// that a lot of the functions can be simplified and made more elegant. Revisit
// after other audio cleanup is done. (crbug.com/120319)

#include "media/audio/audio_util.h"

#include <algorithm>
#include <limits>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/media_switches.h"

#if defined(OS_MACOSX)
#include "media/audio/mac/audio_low_latency_input_mac.h"
#include "media/audio/mac/audio_low_latency_output_mac.h"
#elif defined(OS_WIN)
#include "base/win/windows_version.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/win/audio_low_latency_input_win.h"
#include "media/audio/win/audio_low_latency_output_win.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/base/limits.h"
#endif

namespace media {

// Returns user buffer size as specified on the command line or 0 if no buffer
// size has been specified.
static int GetUserBufferSize() {
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  int buffer_size = 0;
  std::string buffer_size_str(cmd_line->GetSwitchValueASCII(
      switches::kAudioBufferSize));
  if (base::StringToInt(buffer_size_str, &buffer_size) && buffer_size > 0) {
    return buffer_size;
  }

  return 0;
}

// TODO(fbarchard): Convert to intrinsics for better efficiency.
template<class Fixed>
static int ScaleChannel(int channel, int volume) {
  return static_cast<int>((static_cast<Fixed>(channel) * volume) >> 16);
}

template<class Format, class Fixed, int bias>
static void AdjustVolume(Format* buf_out,
                         int sample_count,
                         int fixed_volume) {
  for (int i = 0; i < sample_count; ++i) {
    buf_out[i] = static_cast<Format>(ScaleChannel<Fixed>(buf_out[i] - bias,
                                                         fixed_volume) + bias);
  }
}

// AdjustVolume() does an in place audio sample change.
bool AdjustVolume(void* buf,
                  size_t buflen,
                  int channels,
                  int bytes_per_sample,
                  float volume) {
  DCHECK(buf);
  if (volume < 0.0f || volume > 1.0f)
    return false;
  if (volume == 1.0f) {
    return true;
  } else if (volume == 0.0f) {
    memset(buf, 0, buflen);
    return true;
  }
  if (channels > 0 && channels <= 8 && bytes_per_sample > 0) {
    int sample_count = buflen / bytes_per_sample;
    const int fixed_volume = static_cast<int>(volume * 65536);
    if (bytes_per_sample == 1) {
      AdjustVolume<uint8, int32, 128>(reinterpret_cast<uint8*>(buf),
                                      sample_count,
                                      fixed_volume);
      return true;
    } else if (bytes_per_sample == 2) {
      AdjustVolume<int16, int32, 0>(reinterpret_cast<int16*>(buf),
                                    sample_count,
                                    fixed_volume);
      return true;
    } else if (bytes_per_sample == 4) {
      AdjustVolume<int32, int64, 0>(reinterpret_cast<int32*>(buf),
                                    sample_count,
                                    fixed_volume);
      return true;
    }
  }
  return false;
}

int GetAudioHardwareSampleRate() {
#if defined(OS_MACOSX)
  // Hardware sample-rate on the Mac can be configured, so we must query.
  return AUAudioOutputStream::HardwareSampleRate();
#elif defined(OS_WIN)
  if (!CoreAudioUtil::IsSupported()) {
    // Fall back to Windows Wave implementation on Windows XP or lower
    // and use 48kHz as default input sample rate.
    return 48000;
  }

  // TODO(crogers): tune this rate for best possible WebAudio performance.
  // WebRTC works well at 48kHz and a buffer size of 480 samples will be used
  // for this case. Note that exclusive mode is experimental.
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableExclusiveAudio)) {
    // This sample rate will be combined with a buffer size of 256 samples
    // (see GetAudioHardwareBufferSize()), which corresponds to an output
    // delay of ~5.33ms.
    return 48000;
  }

  // Hardware sample-rate on Windows can be configured, so we must query.
  // TODO(henrika): improve possibility to specify an audio endpoint.
  // Use the default device (same as for Wave) for now to be compatible.
  return WASAPIAudioOutputStream::HardwareSampleRate();
#elif defined(OS_ANDROID)
  // TODO(leozwang): return native sampling rate on Android.
  return 16000;
#else
  return 48000;
#endif
}

int GetAudioInputHardwareSampleRate(const std::string& device_id) {
  // TODO(henrika): add support for device selection on all platforms.
  // Only exists on Windows today.
#if defined(OS_MACOSX)
  return AUAudioInputStream::HardwareSampleRate();
#elif defined(OS_WIN)
  if (!CoreAudioUtil::IsSupported()) {
    return 48000;
  }
  return WASAPIAudioInputStream::HardwareSampleRate(device_id);
#elif defined(OS_ANDROID)
  return 16000;
#else
  // Hardware for Linux is nearly always 48KHz.
  // TODO(crogers) : return correct value in rare non-48KHz cases.
  return 48000;
#endif
}

size_t GetAudioHardwareBufferSize() {
  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    return user_buffer_size;

  // The sizes here were determined by experimentation and are roughly
  // the lowest value (for low latency) that still allowed glitch-free
  // audio under high loads.
  //
  // For Mac OS X and Windows the chromium audio backend uses a low-latency
  // Core Audio API, so a low buffer size is possible. For Linux, further
  // tuning may be needed.
#if defined(OS_MACOSX)
  return 128;
#elif defined(OS_WIN)
  // TODO(henrika): resolve conflict with GetUserBufferSize().
  // If the user tries to set a buffer size using GetUserBufferSize() it will
  // most likely fail since only the native/perfect buffer size is allowed.

  // Buffer size to use when a proper size can't be determined from the system.
  static const int kFallbackBufferSize = 2048;

  if (!CoreAudioUtil::IsSupported()) {
    // Fall back to Windows Wave implementation on Windows XP or lower
    // and assume 48kHz as default sample rate.
    return kFallbackBufferSize;
  }

  // TODO(crogers): tune this size to best possible WebAudio performance.
  // WebRTC always uses 10ms for Windows and does not call this method.
  // Note that exclusive mode is experimental.
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableExclusiveAudio)) {
    return 256;
  }

  AudioParameters params;
  HRESULT hr = CoreAudioUtil::GetPreferredAudioParameters(eRender, eConsole,
                                                          &params);
  return FAILED(hr) ? kFallbackBufferSize : params.frames_per_buffer();
#elif defined(OS_LINUX)
  return 512;
#else
  return 2048;
#endif
}

ChannelLayout GetAudioInputHardwareChannelLayout(const std::string& device_id) {
  // TODO(henrika): add support for device selection on all platforms.
  // Only exists on Windows today.
#if defined(OS_MACOSX)
  return CHANNEL_LAYOUT_MONO;
#elif defined(OS_WIN)
  if (!CoreAudioUtil::IsSupported()) {
    // Fall back to Windows Wave implementation on Windows XP or lower and
    // use stereo by default.
    return CHANNEL_LAYOUT_STEREO;
  }
  return WASAPIAudioInputStream::HardwareChannelCount(device_id) == 1 ?
      CHANNEL_LAYOUT_MONO : CHANNEL_LAYOUT_STEREO;
#else
  return CHANNEL_LAYOUT_STEREO;
#endif
}

// Computes a buffer size based on the given |sample_rate|. Must be used in
// conjunction with AUDIO_PCM_LINEAR.
size_t GetHighLatencyOutputBufferSize(int sample_rate) {
  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    return user_buffer_size;

  // TODO(vrk/crogers): The buffer sizes that this function computes is probably
  // overly conservative. However, reducing the buffer size to 2048-8192 bytes
  // caused crbug.com/108396. This computation should be revisited while making
  // sure crbug.com/108396 doesn't happen again.

  // The minimum number of samples in a hardware packet.
  // This value is selected so that we can handle down to 5khz sample rate.
  static const size_t kMinSamplesPerHardwarePacket = 1024;

  // The maximum number of samples in a hardware packet.
  // This value is selected so that we can handle up to 192khz sample rate.
  static const size_t kMaxSamplesPerHardwarePacket = 64 * 1024;

  // This constant governs the hardware audio buffer size, this value should be
  // chosen carefully.
  // This value is selected so that we have 8192 samples for 48khz streams.
  static const size_t kMillisecondsPerHardwarePacket = 170;

  // Select the number of samples that can provide at least
  // |kMillisecondsPerHardwarePacket| worth of audio data.
  size_t samples = kMinSamplesPerHardwarePacket;
  while (samples <= kMaxSamplesPerHardwarePacket &&
         samples * base::Time::kMillisecondsPerSecond <
         sample_rate * kMillisecondsPerHardwarePacket) {
    samples *= 2;
  }
  return samples;
}

#if defined(OS_WIN)

int NumberOfWaveOutBuffers() {
  // Use the user provided buffer count if provided.
  int buffers = 0;
  std::string buffers_str(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
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

#endif

}  // namespace media
