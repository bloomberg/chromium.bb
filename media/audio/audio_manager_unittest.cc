// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_PULSEAUDIO)
#include "media/audio/pulse/audio_manager_pulse.h"
#endif

namespace media {

// TODO(joi): Remove guards once implemented for all platforms.
TEST(AudioManagerTest, GetAudioOutputDeviceNames) {
#if defined(USE_PULSEAUDIO)
  scoped_ptr<AudioManager> audio_manager_pulse(AudioManagerPulse::Create());
  if (!audio_manager_pulse)
    return;

  AudioDeviceNames device_names;
  audio_manager_pulse->GetAudioOutputDeviceNames(&device_names);

  VLOG(2) << "Got " << device_names.size() << " audio output devices.";
  for (AudioDeviceNames::iterator it = device_names.begin();
       it != device_names.end();
       ++it) {
    EXPECT_FALSE(it->unique_id.empty());
    EXPECT_FALSE(it->device_name.empty());
    VLOG(2) << "Device ID(" << it->unique_id << "), label: " << it->device_name;
  }
#endif  // defined(USE_PULSEAUDIO)
}

}  // namespace media
