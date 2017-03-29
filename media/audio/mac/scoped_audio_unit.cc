// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/scoped_audio_unit.h"

#include "base/mac/mac_logging.h"

namespace media {

constexpr AudioComponentDescription desc = {kAudioUnitType_Output,
                                            kAudioUnitSubType_HALOutput,
                                            kAudioUnitManufacturer_Apple, 0, 0};

static void DestroyAudioUnit(AudioUnit audio_unit) {
  OSStatus result = AudioUnitUninitialize(audio_unit);
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioUnitUninitialize() failed : " << audio_unit;
  result = AudioComponentInstanceDispose(audio_unit);
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioComponentInstanceDispose() failed : " << audio_unit;
}

ScopedAudioUnit::ScopedAudioUnit(AudioDeviceID device, AUElement element) {
  AudioComponent comp = AudioComponentFindNext(0, &desc);
  if (!comp)
    return;

  AudioUnit audio_unit;
  OSStatus result = AudioComponentInstanceNew(comp, &audio_unit);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result) << "AudioComponentInstanceNew() failed.";
    return;
  }

  result = AudioUnitSetProperty(
      audio_unit, kAudioOutputUnitProperty_CurrentDevice,
      kAudioUnitScope_Global, element, &device, sizeof(AudioDeviceID));
  if (result == noErr) {
    audio_unit_ = audio_unit;
    return;
  }
  OSSTATUS_DLOG(ERROR, result)
      << "Failed to set current device for audio unit.";
  DestroyAudioUnit(audio_unit);
}

ScopedAudioUnit::~ScopedAudioUnit() {
  if (audio_unit_)
    DestroyAudioUnit(audio_unit_);
}

}  // namespace media
