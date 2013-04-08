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

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "media/base/media_switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace media {

// Returns user buffer size as specified on the command line or 0 if no buffer
// size has been specified.
int GetUserBufferSize() {
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  int buffer_size = 0;
  std::string buffer_size_str(cmd_line->GetSwitchValueASCII(
      switches::kAudioBufferSize));
  if (base::StringToInt(buffer_size_str, &buffer_size) && buffer_size > 0)
    return buffer_size;

  return 0;
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
