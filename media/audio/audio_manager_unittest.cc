// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_LINUX)
#include "media/audio/linux/audio_manager_linux.h"
#endif  // defined(OS_LINUX)

#if defined(USE_PULSEAUDIO)
#include "media/audio/pulse/audio_manager_pulse.h"
#endif  // defined(USE_PULSEAUDIO)

namespace media {

void GetAudioOutputDeviceNamesImpl(AudioManager* audio_manager) {
  AudioDeviceNames device_names;
  audio_manager->GetAudioOutputDeviceNames(&device_names);

  VLOG(2) << "Got " << device_names.size() << " audio output devices.";
  for (AudioDeviceNames::iterator it = device_names.begin();
       it != device_names.end();
       ++it) {
    EXPECT_FALSE(it->unique_id.empty());
    EXPECT_FALSE(it->device_name.empty());
    VLOG(2) << "Device ID(" << it->unique_id << "), label: " << it->device_name;
  }
}

TEST(AudioManagerTest, GetAudioOutputDeviceNames) {
#if defined(USE_PULSEAUDIO)
  {
    VLOG(2) << "Testing AudioManagerPulse.";
    scoped_ptr<AudioManager> pulse_audio_manager(AudioManagerPulse::Create());
    if (pulse_audio_manager.get())
      GetAudioOutputDeviceNamesImpl(pulse_audio_manager.get());
    else
      LOG(WARNING) << "No pulseaudio on this system.";
  }
#endif  // defined(USE_PULSEAUDIO)
#if defined(USE_ALSA)
  {
    VLOG(2) << "Testing AudioManagerLinux.";
    scoped_ptr<AudioManager> alsa_audio_manager(new AudioManagerLinux());
    GetAudioOutputDeviceNamesImpl(alsa_audio_manager.get());
  }
#endif  // defined(USE_ALSA)
}

}  // namespace media
