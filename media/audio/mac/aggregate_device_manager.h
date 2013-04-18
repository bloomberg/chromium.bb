// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AGGREGATE_DEVICE_MANAGER_H_
#define MEDIA_AUDIO_MAC_AGGREGATE_DEVICE_MANAGER_H_

#include <CoreAudio/CoreAudio.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT AggregateDeviceManager {
 public:
  AggregateDeviceManager();
  ~AggregateDeviceManager();

  // Lazily creates an aggregate device based on the default
  // input and output devices.
  // It will either return a valid device or kAudioDeviceUnknown
  // if the default devices are not suitable for aggregate devices.
  AudioDeviceID GetDefaultAggregateDevice();

 private:
  // The caller is responsible for releasing the CFStringRef.
  static CFStringRef GetDeviceUID(AudioDeviceID id);

  static void GetDeviceName(AudioDeviceID id, char* name, UInt32 size);
  static UInt32 GetClockDomain(AudioDeviceID device_id);
  static OSStatus GetPluginID(AudioObjectID* id);

  CFMutableDictionaryRef CreateAggregateDeviceDictionary(
      AudioDeviceID input_id,
      AudioDeviceID output_id);

  CFMutableArrayRef CreateSubDeviceArray(CFStringRef input_device_UID,
                                         CFStringRef output_device_UID);

  OSStatus CreateAggregateDevice(AudioDeviceID input_id,
                                 AudioDeviceID output_id,
                                 AudioDeviceID* aggregate_device);
  void DestroyAggregateDevice();

  AudioObjectID plugin_id_;
  AudioDeviceID input_device_;
  AudioDeviceID output_device_;

  AudioDeviceID aggregate_device_;

  DISALLOW_COPY_AND_ASSIGN(AggregateDeviceManager);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AGGREGATE_DEVICE_MANAGER_H_
