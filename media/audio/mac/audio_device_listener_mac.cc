// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_device_listener_mac.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/message_loop/message_loop.h"
#include "base/pending_task.h"

namespace media {

// Property property to monitor for device changes.
const AudioObjectPropertyAddress
    AudioDeviceListenerMac::kDefaultOutputDeviceChangePropertyAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};

const AudioObjectPropertyAddress
    AudioDeviceListenerMac::kDefaultInputDeviceChangePropertyAddress = {
        kAudioHardwarePropertyDefaultInputDevice,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};

const AudioObjectPropertyAddress
    AudioDeviceListenerMac::kDevicesPropertyAddress = {
        kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster};

class AudioDeviceListenerMac::PropertyListener {
 public:
  PropertyListener(const AudioObjectPropertyAddress* property,
                   AudioDeviceListenerMac* device_listener)
      : address_(property), device_listener_(device_listener) {}

  base::RepeatingClosure& callback() { return device_listener_->listener_cb_; }
  const AudioObjectPropertyAddress* property() { return address_; }

 private:
  const AudioObjectPropertyAddress* address_;
  AudioDeviceListenerMac* device_listener_;
};

// Callback from the system when an event occurs; this must be called on the
// MessageLoop that created the AudioManager.
// static
OSStatus AudioDeviceListenerMac::OnEvent(
    AudioObjectID object,
    UInt32 num_addresses,
    const AudioObjectPropertyAddress addresses[],
    void* context) {
  if (object != kAudioObjectSystemObject)
    return noErr;

  PropertyListener* listener = static_cast<PropertyListener*>(context);
  for (UInt32 i = 0; i < num_addresses; ++i) {
    if (addresses[i].mSelector == listener->property()->mSelector &&
        addresses[i].mScope == listener->property()->mScope &&
        addresses[i].mElement == listener->property()->mElement && context) {
      listener->callback().Run();
      break;
    }
  }

  return noErr;
}

AudioDeviceListenerMac::AudioDeviceListenerMac(
    const base::RepeatingClosure listener_cb,
    bool monitor_default_input,
    bool monitor_addition_removal) {
  listener_cb_ = std::move(listener_cb);

  // Changes to the default output device are always monitored.
  default_output_listener_ = std::make_unique<PropertyListener>(
      &kDefaultOutputDeviceChangePropertyAddress, this);
  if (!AddPropertyListener(default_output_listener_.get()))
    default_output_listener_.reset();

  if (monitor_default_input) {
    default_input_listener_ = std::make_unique<PropertyListener>(
        &kDefaultInputDeviceChangePropertyAddress, this);
    if (!AddPropertyListener(default_input_listener_.get()))
      default_input_listener_.reset();
  }

  if (monitor_addition_removal) {
    addition_removal_listener_ =
        std::make_unique<PropertyListener>(&kDevicesPropertyAddress, this);
    if (!AddPropertyListener(addition_removal_listener_.get()))
      addition_removal_listener_.reset();
  }
}

AudioDeviceListenerMac::~AudioDeviceListenerMac() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Since we're running on the same CFRunLoop, there can be no outstanding
  // callbacks in flight.
  if (default_output_listener_)
    RemovePropertyListener(default_output_listener_.get());
  if (default_input_listener_)
    RemovePropertyListener(default_input_listener_.get());
  if (addition_removal_listener_)
    RemovePropertyListener(addition_removal_listener_.get());
}

bool AudioDeviceListenerMac::AddPropertyListener(
    AudioDeviceListenerMac::PropertyListener* property_listener) {
  OSStatus result = AudioObjectAddPropertyListener(
      kAudioObjectSystemObject, property_listener->property(),
      &AudioDeviceListenerMac::OnEvent, property_listener);
  bool success = result == noErr;
  if (!success)
    OSSTATUS_DLOG(ERROR, result) << "AudioObjectAddPropertyListener() failed!";

  return success;
}

void AudioDeviceListenerMac::RemovePropertyListener(
    AudioDeviceListenerMac::PropertyListener* property_listener) {
  OSStatus result = AudioObjectRemovePropertyListener(
      kAudioObjectSystemObject, property_listener->property(),
      &AudioDeviceListenerMac::OnEvent, property_listener);
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioObjectRemovePropertyListener() failed!";
}

}  // namespace media
