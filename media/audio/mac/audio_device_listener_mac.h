// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_

#include <CoreAudio/AudioHardware.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"

namespace media {

// AudioDeviceListenerMac facilitates execution of device listener callbacks
// issued via CoreAudio.
class MEDIA_EXPORT AudioDeviceListenerMac {
 public:
  // |listener_cb| will be called when a device change occurs; it's a permanent
  // callback and must outlive AudioDeviceListenerMac.  Note that |listener_cb|
  // might not be executed on the same thread as construction.
  AudioDeviceListenerMac(base::RepeatingClosure listener_cb,
                         bool monitor_default_input = false,
                         bool monitor_addition_removal = false);
  ~AudioDeviceListenerMac();

 private:
  friend class AudioDeviceListenerMacTest;
  class PropertyListener;
  static const AudioObjectPropertyAddress
      kDefaultOutputDeviceChangePropertyAddress;
  static const AudioObjectPropertyAddress
      kDefaultInputDeviceChangePropertyAddress;
  static const AudioObjectPropertyAddress kDevicesPropertyAddress;

  static OSStatus OnEvent(AudioObjectID object,
                          UInt32 num_addresses,
                          const AudioObjectPropertyAddress addresses[],
                          void* context);

  bool AddPropertyListener(PropertyListener* property_listener);
  void RemovePropertyListener(PropertyListener* property_listener);

  base::RepeatingClosure listener_cb_;
  std::unique_ptr<PropertyListener> default_output_listener_;
  std::unique_ptr<PropertyListener> default_input_listener_;
  std::unique_ptr<PropertyListener> addition_removal_listener_;

  // AudioDeviceListenerMac must be constructed and destructed on the same
  // thread.
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceListenerMac);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_
