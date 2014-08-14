// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/mac/video_capture_device_factory_mac.h"

#import <IOKit/audio/IOAudioTypes.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#import "media/video/capture/mac/avfoundation_glue.h"
#include "media/video/capture/mac/video_capture_device_mac.h"
#import "media/video/capture/mac/video_capture_device_avfoundation_mac.h"
#import "media/video/capture/mac/video_capture_device_qtkit_mac.h"

namespace media {

// Some devices are not correctly supported in AVFoundation, f.i. Blackmagic,
// see http://crbug.com/347371. The devices are identified by a characteristic
// trailing substring of uniqueId and by (part of) the vendor's name.
// Blackmagic cameras are known to crash if VGA is requested , see
// http://crbug.com/396812; for them HD is the only supported resolution.
const struct NameAndVid {
  const char* unique_id_signature;
  const char* name;
  const int capture_width;
  const int capture_height;
  const float capture_frame_rate;
} kBlacklistedCameras[] = {
  { "-01FDA82C8A9C", "Blackmagic", 1280, 720, 60.0f } };

static scoped_ptr<media::VideoCaptureDevice::Names>
EnumerateDevicesUsingQTKit() {
  scoped_ptr<VideoCaptureDevice::Names> device_names(
        new VideoCaptureDevice::Names());
  NSMutableDictionary* capture_devices =
      [[[NSMutableDictionary alloc] init] autorelease];
  [VideoCaptureDeviceQTKit getDeviceNames:capture_devices];
  for (NSString* key in capture_devices) {
    VideoCaptureDevice::Name name(
        [[[capture_devices valueForKey:key] deviceName] UTF8String],
        [key UTF8String], VideoCaptureDevice::Name::QTKIT);
    for (size_t i = 0; i < arraysize(kBlacklistedCameras); ++i) {
      if (name.id().find(kBlacklistedCameras[i].name) != std::string::npos) {
        DVLOG(2) << "Found blacklisted camera: " << name.id();
        name.set_is_blacklisted(true);
        break;
      }
    }
    device_names->push_back(name);
  }
  return device_names.Pass();
}

static void RunDevicesEnumeratedCallback(
    const base::Callback<void(scoped_ptr<media::VideoCaptureDevice::Names>)>&
        callback,
    scoped_ptr<media::VideoCaptureDevice::Names> device_names) {
  callback.Run(device_names.Pass());
}

// static
bool VideoCaptureDeviceFactoryMac::PlatformSupportsAVFoundation() {
  return AVFoundationGlue::IsAVFoundationSupported();
}

VideoCaptureDeviceFactoryMac::VideoCaptureDeviceFactoryMac(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : ui_task_runner_(ui_task_runner) {
  thread_checker_.DetachFromThread();
}

VideoCaptureDeviceFactoryMac::~VideoCaptureDeviceFactoryMac() {}

scoped_ptr<VideoCaptureDevice> VideoCaptureDeviceFactoryMac::Create(
    const VideoCaptureDevice::Name& device_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(device_name.capture_api_type(),
            VideoCaptureDevice::Name::API_TYPE_UNKNOWN);

  // Check device presence only for AVFoundation API, since it is too expensive
  // and brittle for QTKit. The actual initialization at device level will fail
  // subsequently if the device is not present.
  if (AVFoundationGlue::IsAVFoundationSupported()) {
    scoped_ptr<VideoCaptureDevice::Names> device_names(
        new VideoCaptureDevice::Names());
    GetDeviceNames(device_names.get());

    VideoCaptureDevice::Names::iterator it = device_names->begin();
    for (; it != device_names->end(); ++it) {
      if (it->id() == device_name.id())
        break;
    }
    if (it == device_names->end())
      return scoped_ptr<VideoCaptureDevice>();
  }

  scoped_ptr<VideoCaptureDeviceMac> capture_device(
      new VideoCaptureDeviceMac(device_name));
  if (!capture_device->Init(device_name.capture_api_type())) {
    LOG(ERROR) << "Could not initialize VideoCaptureDevice.";
    capture_device.reset();
  }
  return scoped_ptr<VideoCaptureDevice>(capture_device.Pass());
}

void VideoCaptureDeviceFactoryMac::GetDeviceNames(
    VideoCaptureDevice::Names* device_names) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Loop through all available devices and add to |device_names|.
  NSDictionary* capture_devices;
  if (AVFoundationGlue::IsAVFoundationSupported()) {
    bool is_any_device_blacklisted = false;
    DVLOG(1) << "Enumerating video capture devices using AVFoundation";
    capture_devices = [VideoCaptureDeviceAVFoundation deviceNames];
    // Enumerate all devices found by AVFoundation, translate the info for each
    // to class Name and add it to |device_names|.
    for (NSString* key in capture_devices) {
      int transport_type = [[capture_devices valueForKey:key] transportType];
      // Transport types are defined for Audio devices and reused for video.
      VideoCaptureDevice::Name::TransportType device_transport_type =
          (transport_type == kIOAudioDeviceTransportTypeBuiltIn ||
              transport_type == kIOAudioDeviceTransportTypeUSB)
          ? VideoCaptureDevice::Name::USB_OR_BUILT_IN
          : VideoCaptureDevice::Name::OTHER_TRANSPORT;
      VideoCaptureDevice::Name name(
          [[[capture_devices valueForKey:key] deviceName] UTF8String],
          [key UTF8String], VideoCaptureDevice::Name::AVFOUNDATION,
          device_transport_type);
      device_names->push_back(name);
      for (size_t i = 0; i < arraysize(kBlacklistedCameras); ++i) {
        is_any_device_blacklisted |= EndsWith(name.id(),
            kBlacklistedCameras[i].unique_id_signature, false);
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
        NSString* device_name = [[capture_devices valueForKey:key] deviceName];
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
    // We should not enumerate QTKit devices in Device Thread;
    NOTREACHED();
  }
}

void VideoCaptureDeviceFactoryMac::EnumerateDeviceNames(const base::Callback<
    void(scoped_ptr<media::VideoCaptureDevice::Names>)>& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (AVFoundationGlue::IsAVFoundationSupported()) {
    scoped_ptr<VideoCaptureDevice::Names> device_names(
        new VideoCaptureDevice::Names());
    GetDeviceNames(device_names.get());
    callback.Run(device_names.Pass());
  } else {
    DVLOG(1) << "Enumerating video capture devices using QTKit";
    base::PostTaskAndReplyWithResult(ui_task_runner_, FROM_HERE,
        base::Bind(&EnumerateDevicesUsingQTKit),
        base::Bind(&RunDevicesEnumeratedCallback, callback));
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
    // Blacklisted cameras provide their own supported format(s), otherwise no
    // such information is provided for QTKit.
    if (device.is_blacklisted()) {
      for (size_t i = 0; i < arraysize(kBlacklistedCameras); ++i) {
        if (device.id().find(kBlacklistedCameras[i].name) !=
            std::string::npos) {
          supported_formats->push_back(media::VideoCaptureFormat(
              gfx::Size(kBlacklistedCameras[i].capture_width,
                        kBlacklistedCameras[i].capture_height),
              kBlacklistedCameras[i].capture_frame_rate,
              media::PIXEL_FORMAT_UYVY));
          break;
        }
      }
    }
  }
}

}  // namespace media
