// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/mac/video_capture_device_factory_mac.h"

#import <IOKit/audio/IOAudioTypes.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#import "media/base/mac/avfoundation_glue.h"
#import "media/video/capture/mac/video_capture_device_avfoundation_mac.h"
#include "media/video/capture/mac/video_capture_device_mac.h"
#import "media/video/capture/mac/video_capture_device_decklink_mac.h"
#import "media/video/capture/mac/video_capture_device_qtkit_mac.h"

namespace media {

// In QTKit API, some devices are known to crash if VGA is requested, for them
// HD is the only supported resolution (see http://crbug.com/396812). In the
// AVfoundation case, we skip enumerating them altogether. These devices are
// identified by a characteristic trailing substring of uniqueId. At the moment
// these are just Blackmagic devices.
const struct NameAndVid {
  const char* unique_id_signature;
  const int capture_width;
  const int capture_height;
  const float capture_frame_rate;
} kBlacklistedCameras[] = { {"-01FDA82C8A9C", 1280, 720, 60.0f } };

static bool IsDeviceBlacklisted(const VideoCaptureDevice::Name& name) {
  bool is_device_blacklisted = false;
  for(size_t i = 0;
    !is_device_blacklisted && i < arraysize(kBlacklistedCameras); ++i) {
    is_device_blacklisted = EndsWith(name.id(),
      kBlacklistedCameras[i].unique_id_signature, false);
  }
  DVLOG_IF(2, is_device_blacklisted) << "Blacklisted camera: " <<
      name.name() << ", id: " << name.id();
  return is_device_blacklisted;
}

static scoped_ptr<media::VideoCaptureDevice::Names>
EnumerateDevicesUsingQTKit() {
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/458397 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "458397 media::EnumerateDevicesUsingQTKit"));

  scoped_ptr<VideoCaptureDevice::Names> device_names(
        new VideoCaptureDevice::Names());
  NSMutableDictionary* capture_devices =
      [[[NSMutableDictionary alloc] init] autorelease];
  [VideoCaptureDeviceQTKit getDeviceNames:capture_devices];
  for (NSString* key in capture_devices) {
    VideoCaptureDevice::Name name(
        [[[capture_devices valueForKey:key] deviceName] UTF8String],
        [key UTF8String], VideoCaptureDevice::Name::QTKIT);
    if (IsDeviceBlacklisted(name))
      name.set_is_blacklisted(true);
    device_names->push_back(name);
  }
  return device_names.Pass();
}

static void RunDevicesEnumeratedCallback(
    const base::Callback<void(scoped_ptr<media::VideoCaptureDevice::Names>)>&
        callback,
    scoped_ptr<media::VideoCaptureDevice::Names> device_names) {
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/458397 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "458397 media::RunDevicesEnumeratedCallback"));
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

  scoped_ptr<VideoCaptureDevice> capture_device;
  if (device_name.capture_api_type() == VideoCaptureDevice::Name::DECKLINK) {
    capture_device.reset(new VideoCaptureDeviceDeckLinkMac(device_name));
  } else {
    VideoCaptureDeviceMac* device = new VideoCaptureDeviceMac(device_name);
    capture_device.reset(device);
    if (!device->Init(device_name.capture_api_type())) {
      LOG(ERROR) << "Could not initialize VideoCaptureDevice.";
      capture_device.reset();
    }
  }
  return scoped_ptr<VideoCaptureDevice>(capture_device.Pass());
}

void VideoCaptureDeviceFactoryMac::GetDeviceNames(
    VideoCaptureDevice::Names* device_names) {
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/458397 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "458397 VideoCaptureDeviceFactoryMac::GetDeviceNames"));
  DCHECK(thread_checker_.CalledOnValidThread());
  // Loop through all available devices and add to |device_names|.
  NSDictionary* capture_devices;
  if (AVFoundationGlue::IsAVFoundationSupported()) {
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
      if (IsDeviceBlacklisted(name))
        continue;
      device_names->push_back(name);
    }
    // Also retrieve Blackmagic devices, if present, via DeckLink SDK API.
    VideoCaptureDeviceDeckLinkMac::EnumerateDevices(device_names);
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
    base::PostTaskAndReplyWithResult(ui_task_runner_.get(), FROM_HERE,
        base::Bind(&EnumerateDevicesUsingQTKit),
        base::Bind(&RunDevicesEnumeratedCallback, callback));
  }
}

void VideoCaptureDeviceFactoryMac::GetDeviceSupportedFormats(
    const VideoCaptureDevice::Name& device,
    VideoCaptureFormats* supported_formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  switch (device.capture_api_type()) {
  case VideoCaptureDevice::Name::AVFOUNDATION:
    DVLOG(1) << "Enumerating video capture capabilities, AVFoundation";
    [VideoCaptureDeviceAVFoundation getDevice:device
                             supportedFormats:supported_formats];
    break;
  case VideoCaptureDevice::Name::QTKIT:
    // Blacklisted cameras provide their own supported format(s), otherwise no
    // such information is provided for QTKit devices.
    if (device.is_blacklisted()) {
      for (size_t i = 0; i < arraysize(kBlacklistedCameras); ++i) {
        if (EndsWith(device.id(), kBlacklistedCameras[i].unique_id_signature,
            false)) {
          supported_formats->push_back(media::VideoCaptureFormat(
              gfx::Size(kBlacklistedCameras[i].capture_width,
                        kBlacklistedCameras[i].capture_height),
              kBlacklistedCameras[i].capture_frame_rate,
              media::PIXEL_FORMAT_UYVY));
          break;
        }
      }
    }
    break;
  case VideoCaptureDevice::Name::DECKLINK:
    DVLOG(1) << "Enumerating video capture capabilities " << device.name();
    VideoCaptureDeviceDeckLinkMac::EnumerateDeviceCapabilities(
        device, supported_formats);
    break;
  default:
    NOTREACHED();
  }
}

// static
VideoCaptureDeviceFactory*
VideoCaptureDeviceFactory::CreateVideoCaptureDeviceFactory(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return new VideoCaptureDeviceFactoryMac(ui_task_runner);
}

}  // namespace media
