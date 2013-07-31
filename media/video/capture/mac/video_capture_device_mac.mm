// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/mac/video_capture_device_mac.h"

#import <QTKit/QTKit.h>

#include "base/logging.h"
#include "base/time/time.h"
#include "media/video/capture/mac/video_capture_device_qtkit_mac.h"

namespace {

const int kMinFrameRate = 1;
const int kMaxFrameRate = 30;

// In QT device identifiers, the USB VID and PID are stored in 4 bytes each.
const size_t kVidPidSize = 4;

struct Resolution {
  int width;
  int height;
};

const Resolution kWellSupportedResolutions[] = {
   { 320, 240 },
   { 640, 480 },
   { 1280, 720 },
};

// TODO(ronghuawu): Replace this with CapabilityList::GetBestMatchedCapability.
void GetBestMatchSupportedResolution(int* width, int* height) {
  int min_diff = kint32max;
  int matched_width = *width;
  int matched_height = *height;
  int desired_res_area = *width * *height;
  for (size_t i = 0; i < arraysize(kWellSupportedResolutions); ++i) {
    int area = kWellSupportedResolutions[i].width *
               kWellSupportedResolutions[i].height;
    int diff = std::abs(desired_res_area - area);
    if (diff < min_diff) {
      min_diff = diff;
      matched_width = kWellSupportedResolutions[i].width;
      matched_height = kWellSupportedResolutions[i].height;
    }
  }
  *width = matched_width;
  *height = matched_height;
}

}

namespace media {

void VideoCaptureDevice::GetDeviceNames(Names* device_names) {
  // Loop through all available devices and add to |device_names|.
  device_names->clear();

  NSDictionary* capture_devices = [VideoCaptureDeviceQTKit deviceNames];
  for (NSString* key in capture_devices) {
    Name name([[capture_devices valueForKey:key] UTF8String],
              [key UTF8String]);
    device_names->push_back(name);
  }
}

const std::string VideoCaptureDevice::Name::GetModel() const {
  // Both PID and VID are 4 characters.
  if (unique_id_.size() < 2 * kVidPidSize) {
    return "";
  }

  // The last characters of device id is a concatenation of VID and then PID.
  const size_t vid_location = unique_id_.size() - 2 * kVidPidSize;
  std::string id_vendor = unique_id_.substr(vid_location, kVidPidSize);
  const size_t pid_location = unique_id_.size() - kVidPidSize;
  std::string id_product = unique_id_.substr(pid_location, kVidPidSize);

  return id_vendor + ":" + id_product;
}

VideoCaptureDevice* VideoCaptureDevice::Create(const Name& device_name) {
  VideoCaptureDeviceMac* capture_device =
      new VideoCaptureDeviceMac(device_name);
  if (!capture_device->Init()) {
    LOG(ERROR) << "Could not initialize VideoCaptureDevice.";
    delete capture_device;
    capture_device = NULL;
  }
  return capture_device;
}

VideoCaptureDeviceMac::VideoCaptureDeviceMac(const Name& device_name)
    : device_name_(device_name),
      observer_(NULL),
      state_(kNotInitialized),
      capture_device_(nil) {
}

VideoCaptureDeviceMac::~VideoCaptureDeviceMac() {
  [capture_device_ release];
}

void VideoCaptureDeviceMac::Allocate(
    const VideoCaptureCapability& capture_format,
    EventHandler* observer) {
  if (state_ != kIdle) {
    return;
  }
  int width = capture_format.width;
  int height = capture_format.height;
  int frame_rate = capture_format.frame_rate;

  // QTKit can scale captured frame to any size requested, which would lead to
  // undesired aspect ratio change. Tries to open the camera with a natively
  // supported format and let the client to crop/pad the captured frames.
  GetBestMatchSupportedResolution(&width,
                                  &height);

  observer_ = observer;
  NSString* deviceId =
      [NSString stringWithUTF8String:device_name_.id().c_str()];

  [capture_device_ setFrameReceiver:this];

  if (![capture_device_ setCaptureDevice:deviceId]) {
    SetErrorState("Could not open capture device.");
    return;
  }
  if (frame_rate < kMinFrameRate)
    frame_rate = kMinFrameRate;
  else if (frame_rate > kMaxFrameRate)
    frame_rate = kMaxFrameRate;

  if (![capture_device_ setCaptureHeight:height
                                   width:width
                               frameRate:frame_rate]) {
    SetErrorState("Could not configure capture device.");
    return;
  }

  state_ = kAllocated;
  VideoCaptureCapability current_settings;
  current_settings.color = VideoCaptureCapability::kARGB;
  current_settings.width = width;
  current_settings.height = height;
  current_settings.frame_rate = frame_rate;
  current_settings.expected_capture_delay = 0;
  current_settings.interlaced = false;

  observer_->OnFrameInfo(current_settings);
}

void VideoCaptureDeviceMac::Start() {
  DCHECK_EQ(state_, kAllocated);
  if (![capture_device_ startCapture]) {
    SetErrorState("Could not start capture device.");
    return;
  }
  state_ = kCapturing;
}

void VideoCaptureDeviceMac::Stop() {
  DCHECK_EQ(state_, kCapturing);
  [capture_device_ stopCapture];
  state_ = kAllocated;
}

void VideoCaptureDeviceMac::DeAllocate() {
  if (state_ != kAllocated && state_ != kCapturing) {
    return;
  }
  if (state_ == kCapturing) {
    [capture_device_ stopCapture];
  }
  [capture_device_ setCaptureDevice:nil];
  [capture_device_ setFrameReceiver:nil];

  state_ = kIdle;
}

const VideoCaptureDevice::Name& VideoCaptureDeviceMac::device_name() {
  return device_name_;
}

bool VideoCaptureDeviceMac::Init() {
  DCHECK_EQ(state_, kNotInitialized);

  Names device_names;
  GetDeviceNames(&device_names);
  Name* found = device_names.FindById(device_name_.id());
  if (!found)
    return false;

  capture_device_ =
      [[VideoCaptureDeviceQTKit alloc] initWithFrameReceiver:this];
  if (!capture_device_)
    return false;

  state_ = kIdle;
  return true;
}

void VideoCaptureDeviceMac::ReceiveFrame(
    const uint8* video_frame,
    int video_frame_length,
    const VideoCaptureCapability& frame_info) {
  observer_->OnIncomingCapturedFrame(
      video_frame, video_frame_length, base::Time::Now(), 0, false, false);
}

void VideoCaptureDeviceMac::SetErrorState(const std::string& reason) {
  DLOG(ERROR) << reason;
  state_ = kError;
  observer_->OnError();
}

} // namespace media
