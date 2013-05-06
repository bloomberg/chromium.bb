// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/aggregate_device_manager.h"

#include <CoreAudio/AudioHardware.h>
#include <string>

#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/mac/audio_manager_mac.h"

using base::mac::ScopedCFTypeRef;

namespace media {

AggregateDeviceManager::AggregateDeviceManager()
    : plugin_id_(kAudioObjectUnknown),
      input_device_(kAudioDeviceUnknown),
      output_device_(kAudioDeviceUnknown),
      aggregate_device_(kAudioObjectUnknown) {
}

AggregateDeviceManager::~AggregateDeviceManager() {
  DestroyAggregateDevice();
}

AudioDeviceID AggregateDeviceManager::GetDefaultAggregateDevice() {
  AudioDeviceID current_input_device;
  AudioDeviceID current_output_device;
  AudioManagerMac::GetDefaultInputDevice(&current_input_device);
  AudioManagerMac::GetDefaultOutputDevice(&current_output_device);

  if (AudioManagerMac::HardwareSampleRateForDevice(current_input_device) !=
      AudioManagerMac::HardwareSampleRateForDevice(current_output_device)) {
    // TODO(crogers): with some extra work we can make aggregate devices work
    // if the clock domain is the same but the sample-rate differ.
    // For now we fallback to the synchronized path.
    return kAudioDeviceUnknown;
  }

  // Use a lazily created aggregate device if it's already available
  // and still appropriate.
  if (aggregate_device_ != kAudioObjectUnknown) {
    // TODO(crogers): handle default device changes for synchronized I/O.
    // For now, we check to make sure the default devices haven't changed
    // since we lazily created the aggregate device.
    if (current_input_device == input_device_ &&
        current_output_device == output_device_)
      return aggregate_device_;

    // For now, once lazily created don't attempt to create another
    // aggregate device.
    return kAudioDeviceUnknown;
  }

  input_device_ = current_input_device;
  output_device_ = current_output_device;

  // Only create an aggregrate device if the clock domains match.
  UInt32 input_clockdomain = GetClockDomain(input_device_);
  UInt32 output_clockdomain = GetClockDomain(output_device_);
  DVLOG(1) << "input_clockdomain: " << input_clockdomain;
  DVLOG(1) << "output_clockdomain: " << output_clockdomain;

  if (input_clockdomain == 0 || input_clockdomain != output_clockdomain)
    return kAudioDeviceUnknown;

  OSStatus result = CreateAggregateDevice(
      input_device_,
      output_device_,
      &aggregate_device_);
  if (result != noErr)
    DestroyAggregateDevice();

  return aggregate_device_;
}

CFStringRef AggregateDeviceManager::GetDeviceUID(AudioDeviceID id) {
  static const AudioObjectPropertyAddress kDeviceUIDAddress = {
    kAudioDevicePropertyDeviceUID,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  // As stated in the CoreAudio header (AudioHardwareBase.h),
  // the caller is responsible for releasing the device_UID.
  CFStringRef device_UID;
  UInt32 size = sizeof(device_UID);
  OSStatus result = AudioObjectGetPropertyData(
      id,
      &kDeviceUIDAddress,
      0,
      0,
      &size,
      &device_UID);

  return (result == noErr) ? device_UID : NULL;
}

void AggregateDeviceManager::GetDeviceName(
    AudioDeviceID id, char* name, UInt32 size) {
  static const AudioObjectPropertyAddress kDeviceNameAddress = {
    kAudioDevicePropertyDeviceName,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  OSStatus result = AudioObjectGetPropertyData(
      id,
      &kDeviceNameAddress,
      0,
      0,
      &size,
      name);

  if (result != noErr && size > 0)
    name[0] = 0;
}

UInt32 AggregateDeviceManager::GetClockDomain(AudioDeviceID device_id) {
  static const AudioObjectPropertyAddress kClockDomainAddress = {
    kAudioDevicePropertyClockDomain,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  UInt32 clockdomain = 0;
  UInt32 size = sizeof(UInt32);
  OSStatus result = AudioObjectGetPropertyData(
      device_id,
      &kClockDomainAddress,
      0,
      0,
      &size,
      &clockdomain);

  return (result == noErr) ? clockdomain : 0;
}

OSStatus AggregateDeviceManager::GetPluginID(AudioObjectID* id) {
  DCHECK(id);

  // Get the audio hardware plugin.
  CFStringRef bundle_name = CFSTR("com.apple.audio.CoreAudio");

  AudioValueTranslation plugin_translation;
  plugin_translation.mInputData = &bundle_name;
  plugin_translation.mInputDataSize = sizeof(bundle_name);
  plugin_translation.mOutputData = id;
  plugin_translation.mOutputDataSize = sizeof(*id);

  static const AudioObjectPropertyAddress kPlugInAddress = {
    kAudioHardwarePropertyPlugInForBundleID,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  UInt32 size = sizeof(plugin_translation);
  OSStatus result = AudioObjectGetPropertyData(
      kAudioObjectSystemObject,
      &kPlugInAddress,
      0,
      0,
      &size,
      &plugin_translation);

  DVLOG(1) << "CoreAudio plugin ID: " << *id;

  return result;
}

CFMutableDictionaryRef
AggregateDeviceManager::CreateAggregateDeviceDictionary(
    AudioDeviceID input_id,
    AudioDeviceID output_id) {
  CFMutableDictionaryRef aggregate_device_dict = CFDictionaryCreateMutable(
      NULL,
      0,
      &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  if (!aggregate_device_dict)
    return NULL;

  const CFStringRef kAggregateDeviceName =
      CFSTR("ChromeAggregateAudioDevice");
  const CFStringRef kAggregateDeviceUID =
      CFSTR("com.google.chrome.AggregateAudioDevice");

  // Add name and UID of the device to the dictionary.
  CFDictionaryAddValue(
      aggregate_device_dict,
      CFSTR(kAudioAggregateDeviceNameKey),
      kAggregateDeviceName);
  CFDictionaryAddValue(
      aggregate_device_dict,
      CFSTR(kAudioAggregateDeviceUIDKey),
      kAggregateDeviceUID);

  // Add a "private aggregate key" to the dictionary.
  // The 1 value means that the created aggregate device will
  // only be accessible from the process that created it, and
  // won't be visible to outside processes.
  int value = 1;
  ScopedCFTypeRef<CFNumberRef> aggregate_device_number(CFNumberCreate(
      NULL,
      kCFNumberIntType,
      &value));
  CFDictionaryAddValue(
      aggregate_device_dict,
      CFSTR(kAudioAggregateDeviceIsPrivateKey),
      aggregate_device_number);

  return aggregate_device_dict;
}

CFMutableArrayRef
AggregateDeviceManager::CreateSubDeviceArray(
    CFStringRef input_device_UID, CFStringRef output_device_UID) {
  CFMutableArrayRef sub_devices_array = CFArrayCreateMutable(
      NULL,
      0,
      &kCFTypeArrayCallBacks);

  CFArrayAppendValue(sub_devices_array, input_device_UID);
  CFArrayAppendValue(sub_devices_array, output_device_UID);

  return sub_devices_array;
}

OSStatus AggregateDeviceManager::CreateAggregateDevice(
    AudioDeviceID input_id,
    AudioDeviceID output_id,
    AudioDeviceID* aggregate_device) {
  DCHECK(aggregate_device);

  const size_t kMaxDeviceNameLength = 256;

  scoped_ptr<char[]> input_device_name(new char[kMaxDeviceNameLength]);
  GetDeviceName(
      input_id,
      input_device_name.get(),
      sizeof(input_device_name));
  DVLOG(1) << "Input device: \n" << input_device_name;

  scoped_ptr<char[]> output_device_name(new char[kMaxDeviceNameLength]);
  GetDeviceName(
      output_id,
      output_device_name.get(),
      sizeof(output_device_name));
  DVLOG(1) << "Output device: \n" << output_device_name;

  OSStatus result = GetPluginID(&plugin_id_);
  if (result != noErr)
    return result;

  // Create a dictionary for the aggregate device.
  ScopedCFTypeRef<CFMutableDictionaryRef> aggregate_device_dict(
      CreateAggregateDeviceDictionary(input_id, output_id));
  if (!aggregate_device_dict)
    return -1;

  // Create the aggregate device.
  static const AudioObjectPropertyAddress kCreateAggregateDeviceAddress = {
    kAudioPlugInCreateAggregateDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  UInt32 size = sizeof(*aggregate_device);
  result = AudioObjectGetPropertyData(
      plugin_id_,
      &kCreateAggregateDeviceAddress,
      sizeof(aggregate_device_dict),
      &aggregate_device_dict,
      &size,
      aggregate_device);
  if (result != noErr) {
    DLOG(ERROR) << "Error creating aggregate audio device!";
    return result;
  }

  // Set the sub-devices for the aggregate device.
  // In this case we use two: the input and output devices.

  ScopedCFTypeRef<CFStringRef> input_device_UID(GetDeviceUID(input_id));
  ScopedCFTypeRef<CFStringRef> output_device_UID(GetDeviceUID(output_id));
  if (!input_device_UID || !output_device_UID) {
    DLOG(ERROR) << "Error getting audio device UID strings.";
    return -1;
  }

  ScopedCFTypeRef<CFMutableArrayRef> sub_devices_array(
      CreateSubDeviceArray(input_device_UID, output_device_UID));
  if (sub_devices_array == NULL) {
    DLOG(ERROR) << "Error creating sub-devices array.";
    return -1;
  }

  static const AudioObjectPropertyAddress kSetSubDevicesAddress = {
    kAudioAggregateDevicePropertyFullSubDeviceList,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  size = sizeof(CFMutableArrayRef);
  result = AudioObjectSetPropertyData(
      *aggregate_device,
      &kSetSubDevicesAddress,
      0,
      NULL,
      size,
      &sub_devices_array);
  if (result != noErr) {
    DLOG(ERROR) << "Error setting aggregate audio device sub-devices!";
    return result;
  }

  // Use the input device as the master device.
  static const AudioObjectPropertyAddress kSetMasterDeviceAddress = {
    kAudioAggregateDevicePropertyMasterSubDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  size = sizeof(CFStringRef);
  result = AudioObjectSetPropertyData(
      *aggregate_device,
      &kSetMasterDeviceAddress,
      0,
      NULL,
      size,
      &input_device_UID);
  if (result != noErr) {
    DLOG(ERROR) << "Error setting aggregate audio device master device!";
    return result;
  }

  DVLOG(1) << "New aggregate device: " << *aggregate_device;
  return noErr;
}

void AggregateDeviceManager::DestroyAggregateDevice() {
  if (aggregate_device_ == kAudioObjectUnknown)
    return;

  static const AudioObjectPropertyAddress kDestroyAddress = {
    kAudioPlugInDestroyAggregateDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  UInt32 size = sizeof(aggregate_device_);
  OSStatus result = AudioObjectGetPropertyData(
      plugin_id_,
      &kDestroyAddress,
      0,
      NULL,
      &size,
      &aggregate_device_);
  if (result != noErr) {
    DLOG(ERROR) << "Error destroying aggregate audio device!";
    return;
  }

  aggregate_device_ = kAudioObjectUnknown;
}

}  // namespace media
