// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/mac/video_capture_device_mac.h"

#import <QTKit/QTKit.h>

#include "base/bind.h"
#include "base/location.h"
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

const Resolution kQVGA = { 320, 240 },
                 kVGA = { 640, 480 },
                 kHD = { 1280, 720 };

const Resolution* const kWellSupportedResolutions[] = {
   &kQVGA,
   &kVGA,
   &kHD,
};

// TODO(ronghuawu): Replace this with CapabilityList::GetBestMatchedCapability.
void GetBestMatchSupportedResolution(int* width, int* height) {
  int min_diff = kint32max;
  int matched_width = *width;
  int matched_height = *height;
  int desired_res_area = *width * *height;
  for (size_t i = 0; i < arraysize(kWellSupportedResolutions); ++i) {
    int area = kWellSupportedResolutions[i]->width *
               kWellSupportedResolutions[i]->height;
    int diff = std::abs(desired_res_area - area);
    if (diff < min_diff) {
      min_diff = diff;
      matched_width = kWellSupportedResolutions[i]->width;
      matched_height = kWellSupportedResolutions[i]->height;
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
      sent_frame_info_(false),
      loop_proxy_(base::MessageLoopProxy::current()),
      state_(kNotInitialized),
      weak_factory_(this),
      weak_this_(weak_factory_.GetWeakPtr()),
      capture_device_(nil) {
}

VideoCaptureDeviceMac::~VideoCaptureDeviceMac() {
  DCHECK_EQ(loop_proxy_, base::MessageLoopProxy::current());
  [capture_device_ release];
}

void VideoCaptureDeviceMac::Allocate(
    const VideoCaptureCapability& capture_format,
    EventHandler* observer) {
  DCHECK_EQ(loop_proxy_, base::MessageLoopProxy::current());
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

  current_settings_.color = PIXEL_FORMAT_UYVY;
  current_settings_.width = width;
  current_settings_.height = height;
  current_settings_.frame_rate = frame_rate;
  current_settings_.expected_capture_delay = 0;
  current_settings_.interlaced = false;

  if (width != kHD.width || height != kHD.height) {
    // If the resolution is VGA or QVGA, set the capture resolution to the
    // target size. For most cameras (though not all), at these resolutions
    // QTKit produces frames with square pixels.
    if (!UpdateCaptureResolution())
      return;

    sent_frame_info_ = true;
    observer_->OnFrameInfo(current_settings_);
  }

  // If the resolution is HD, start capturing without setting a resolution.
  // QTKit will produce frames at the native resolution, allowing us to
  // identify cameras whose native resolution is too low for HD.  This
  // additional information comes at a cost in startup latency, because the
  // webcam will need to be reopened if its default resolution is not HD or VGA.

  if (![capture_device_ startCapture]) {
    SetErrorState("Could not start capture device.");
    return;
  }

  state_ = kAllocated;
}

void VideoCaptureDeviceMac::Start() {
  DCHECK_EQ(loop_proxy_, base::MessageLoopProxy::current());
  DCHECK_EQ(state_, kAllocated);
  state_ = kCapturing;

  // This method no longer has any effect.  Capturing is triggered by
  // the call to Allocate.
  // TODO(bemasc, ncarter): Remove this method.
}

void VideoCaptureDeviceMac::Stop() {
  DCHECK_EQ(loop_proxy_, base::MessageLoopProxy::current());
  DCHECK(state_ == kCapturing || state_ == kError) << state_;
  [capture_device_ stopCapture];
  state_ = kAllocated;
}

void VideoCaptureDeviceMac::DeAllocate() {
  DCHECK_EQ(loop_proxy_, base::MessageLoopProxy::current());
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
  DCHECK_EQ(loop_proxy_, base::MessageLoopProxy::current());
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
    const VideoCaptureCapability& frame_info,
    int aspect_numerator,
    int aspect_denominator) {
  // This method is safe to call from a device capture thread,
  // i.e. any thread controlled by QTKit.

  if (!sent_frame_info_) {
    if (current_settings_.width == kHD.width &&
        current_settings_.height == kHD.height) {
      bool changeToVga = false;
      if (frame_info.width < kHD.width || frame_info.height < kHD.height) {
        // These are the default capture settings, not yet configured to match
        // |current_settings_|.
        DCHECK(frame_info.frame_rate == 0);
        DVLOG(1) << "Switching to VGA because the default resolution is " <<
            frame_info.width << "x" << frame_info.height;
        changeToVga = true;
      }
      if (frame_info.width == kHD.width && frame_info.height == kHD.height &&
          aspect_numerator != aspect_denominator) {
        DVLOG(1) << "Switching to VGA because HD has nonsquare pixel " <<
            "aspect ratio " << aspect_numerator << ":" << aspect_denominator;
        changeToVga = true;
      }

      if (changeToVga) {
        current_settings_.width = kVGA.width;
        current_settings_.height = kVGA.height;
      }
    }

    if (current_settings_.width == frame_info.width &&
        current_settings_.height == frame_info.height) {
      sent_frame_info_ = true;
      observer_->OnFrameInfo(current_settings_);
    } else {
      UpdateCaptureResolution();
      // The current frame does not have the right width and height, so it
      // must not be passed to |observer_|.
      return;
    }
  }

  DCHECK(current_settings_.width == frame_info.width &&
         current_settings_.height == frame_info.height);

  observer_->OnIncomingCapturedFrame(
      video_frame, video_frame_length, base::Time::Now(), 0, false, false);
}

void VideoCaptureDeviceMac::ReceiveError(const std::string& reason) {
  loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureDeviceMac::SetErrorState, weak_this_,
          reason));
}

void VideoCaptureDeviceMac::SetErrorState(const std::string& reason) {
  DCHECK_EQ(loop_proxy_, base::MessageLoopProxy::current());
  DLOG(ERROR) << reason;
  state_ = kError;
  observer_->OnError();
}

bool VideoCaptureDeviceMac::UpdateCaptureResolution() {
 if (![capture_device_ setCaptureHeight:current_settings_.height
                                  width:current_settings_.width
                              frameRate:current_settings_.frame_rate]) {
   ReceiveError("Could not configure capture device.");
   return false;
 }
 return true;
}

} // namespace media
