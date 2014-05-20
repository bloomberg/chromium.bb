// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/mac/video_capture_device_factory_mac.h"

#import "media/video/capture/mac/avfoundation_glue.h"
#include "media/video/capture/mac/video_capture_device_mac.h"
#import "media/video/capture/mac/video_capture_device_avfoundation_mac.h"
#import "media/video/capture/mac/video_capture_device_qtkit_mac.h"

namespace media {

// Some devices are not correctly supported in AVFoundation, f.i. Blackmagic,
// see http://crbug.com/347371. The devices are identified by USB Vendor ID and
// by a characteristic substring of the name, usually the vendor's name.
const struct NameAndVid {
  const char* vid;
  const char* name;
} kBlacklistedCameras[] = { { "a82c", "Blackmagic" } };

// In device identifiers, the USB VID and PID are stored in 4 bytes each.
const size_t kVidPidSize = 4;

VideoCaptureDeviceFactoryMac::VideoCaptureDeviceFactoryMac() {
  thread_checker_.DetachFromThread();
}

scoped_ptr<VideoCaptureDevice> VideoCaptureDeviceFactoryMac::Create(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const VideoCaptureDevice::Name& device_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(device_name.capture_api_type(),
            VideoCaptureDevice::Name::API_TYPE_UNKNOWN);

  VideoCaptureDevice::Names device_names;
  GetDeviceNames(&device_names);
  VideoCaptureDevice::Names::iterator it = device_names.begin();
  for (; it != device_names.end(); ++it) {
    if (it->id() == device_name.id())
      break;
  }
  if (it == device_names.end())
    return scoped_ptr<VideoCaptureDevice>();

  scoped_ptr<VideoCaptureDeviceMac> capture_device(
      new VideoCaptureDeviceMac(device_name));
  if (!capture_device->Init(device_name.capture_api_type())) {
    LOG(ERROR) << "Could not initialize VideoCaptureDevice.";
    capture_device.reset();
  }
  return scoped_ptr<VideoCaptureDevice>(capture_device.Pass());
}

void VideoCaptureDeviceFactoryMac::GetDeviceNames(
    VideoCaptureDevice::Names* const device_names) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Loop through all available devices and add to |device_names|.
  NSDictionary* capture_devices;
  if (AVFoundationGlue::IsAVFoundationSupported()) {
    bool is_any_device_blacklisted = false;
    DVLOG(1) << "Enumerating video capture devices using AVFoundation";
    capture_devices = [VideoCaptureDeviceAVFoundation deviceNames];
    std::string device_vid;
    // Enumerate all devices found by AVFoundation, translate the info for each
    // to class Name and add it to |device_names|.
    for (NSString* key in capture_devices) {
      VideoCaptureDevice::Name name(
          [[capture_devices valueForKey:key] UTF8String],
          [key UTF8String], VideoCaptureDevice::Name::AVFOUNDATION);
      device_names->push_back(name);
      // Extract the device's Vendor ID and compare to all blacklisted ones.
      device_vid = name.GetModel().substr(0, kVidPidSize);
      for (size_t i = 0; i < arraysize(kBlacklistedCameras); ++i) {
        is_any_device_blacklisted |=
            !strcasecmp(device_vid.c_str(), kBlacklistedCameras[i].vid);
        if (is_any_device_blacklisted)
          break;
      }
    }
    // If there is any device blacklisted in the system, walk the QTKit device
    // list and add those devices with a blacklisted name to the |device_names|.
    // AVFoundation and QTKit device lists partially overlap, so add a "QTKit"
    // prefix to the latter ones to distinguish them from the AVFoundation ones.
    if (is_any_device_blacklisted) {
      capture_devices = [VideoCaptureDeviceQTKit deviceNames];
      for (NSString* key in capture_devices) {
        NSString* device_name = [capture_devices valueForKey:key];
        for (size_t i = 0; i < arraysize(kBlacklistedCameras); ++i) {
          if ([device_name rangeOfString:@(kBlacklistedCameras[i].name)
                                 options:NSCaseInsensitiveSearch].length != 0) {
            DVLOG(1) << "Enumerated blacklisted " << [device_name UTF8String];
            VideoCaptureDevice::Name name(
                "QTKit " + std::string([device_name UTF8String]),
                [key UTF8String], VideoCaptureDevice::Name::QTKIT);
            device_names->push_back(name);
          }
        }
      }
    }
  } else {
    DVLOG(1) << "Enumerating video capture devices using QTKit";
    capture_devices = [VideoCaptureDeviceQTKit deviceNames];
    for (NSString* key in capture_devices) {
      VideoCaptureDevice::Name name(
          [[capture_devices valueForKey:key] UTF8String],
          [key UTF8String], VideoCaptureDevice::Name::QTKIT);
      device_names->push_back(name);
    }
  }
}

void VideoCaptureDeviceFactoryMac::GetDeviceSupportedFormats(
    const VideoCaptureDevice::Name& device,
    VideoCaptureFormats* supported_formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device.capture_api_type() == VideoCaptureDevice::Name::AVFOUNDATION) {
    DVLOG(1) << "Enumerating video capture capabilities, AVFoundation";
    [VideoCaptureDeviceAVFoundation getDevice:device
                             supportedFormats:supported_formats];
  } else {
    NOTIMPLEMENTED();
  }
}

}  // namespace media
