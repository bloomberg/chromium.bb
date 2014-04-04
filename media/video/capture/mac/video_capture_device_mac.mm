// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/mac/video_capture_device_mac.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/time/time.h"
#import "media/video/capture/mac/avfoundation_glue.h"
#import "media/video/capture/mac/platform_video_capturing_mac.h"
#import "media/video/capture/mac/video_capture_device_avfoundation_mac.h"
#import "media/video/capture/mac/video_capture_device_qtkit_mac.h"

namespace media {

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

// Rescaling the image to fix the pixel aspect ratio runs the risk of making
// the aspect ratio worse, if QTKit selects a new source mode with a different
// shape.  This constant ensures that we don't take this risk if the current
// aspect ratio is tolerable.
const float kMaxPixelAspectRatio = 1.15;

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

void VideoCaptureDevice::GetDeviceNames(Names* device_names) {
  // Loop through all available devices and add to |device_names|.
  device_names->clear();

  NSDictionary* capture_devices;
  if (AVFoundationGlue::IsAVFoundationSupported()) {
    DVLOG(1) << "Enumerating video capture devices using AVFoundation";
    capture_devices = [VideoCaptureDeviceAVFoundation deviceNames];
  } else {
    DVLOG(1) << "Enumerating video capture devices using QTKit";
    capture_devices = [VideoCaptureDeviceQTKit deviceNames];
  }
  for (NSString* key in capture_devices) {
    Name name([[capture_devices valueForKey:key] UTF8String],
              [key UTF8String]);
    device_names->push_back(name);
  }
}

// static
void VideoCaptureDevice::GetDeviceSupportedFormats(const Name& device,
    VideoCaptureFormats* formats) {
  if (AVFoundationGlue::IsAVFoundationSupported()) {
    DVLOG(1) << "Enumerating video capture capabilities, AVFoundation";
    [VideoCaptureDeviceAVFoundation getDevice:device
                             supportedFormats:formats];
  } else {
    NOTIMPLEMENTED();
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
      tried_to_square_pixels_(false),
      task_runner_(base::MessageLoopProxy::current()),
      state_(kNotInitialized),
      capture_device_(nil),
      weak_factory_(this) {
  final_resolution_selected_ = AVFoundationGlue::IsAVFoundationSupported();
}

VideoCaptureDeviceMac::~VideoCaptureDeviceMac() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  [capture_device_ release];
}

void VideoCaptureDeviceMac::AllocateAndStart(
    const VideoCaptureParams& params,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ != kIdle) {
    return;
  }
  int width = params.requested_format.frame_size.width();
  int height = params.requested_format.frame_size.height();
  int frame_rate = params.requested_format.frame_rate;

  // QTKit API can scale captured frame to any size requested, which would lead
  // to undesired aspect ratio changes. Try to open the camera with a known
  // supported format and let the client crop/pad the captured frames.
  GetBestMatchSupportedResolution(&width, &height);

  client_ = client.Pass();
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

  capture_format_.frame_size.SetSize(width, height);
  capture_format_.frame_rate = frame_rate;
  capture_format_.pixel_format = PIXEL_FORMAT_UYVY;

  // QTKit: Set the capture resolution only if this is VGA or smaller, otherwise
  // leave it unconfigured and start capturing: QTKit will produce frames at the
  // native resolution, allowing us to identify cameras whose native resolution
  // is too low for HD. This additional information comes at a cost in startup
  // latency, because the webcam will need to be reopened if its default
  // resolution is not HD or VGA.
  // AVfoundation is configured for all resolutions.
  if (AVFoundationGlue::IsAVFoundationSupported() || width <= kVGA.width ||
      height <= kVGA.height) {
    if (!UpdateCaptureResolution())
      return;
  }
  if (![capture_device_ startCapture]) {
    SetErrorState("Could not start capture device.");
    return;
  }

  state_ = kCapturing;
}

void VideoCaptureDeviceMac::StopAndDeAllocate() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kCapturing || state_ == kError) << state_;
  [capture_device_ stopCapture];

  [capture_device_ setCaptureDevice:nil];
  [capture_device_ setFrameReceiver:nil];
  client_.reset();
  state_ = kIdle;
  tried_to_square_pixels_ = false;
}

bool VideoCaptureDeviceMac::Init() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kNotInitialized);

  // TODO(mcasas): The following check might not be necessary; if the device has
  // disappeared after enumeration and before coming here, opening would just
  // fail but not necessarily produce a crash.
  Names device_names;
  GetDeviceNames(&device_names);
  Names::iterator it = device_names.begin();
  for (; it != device_names.end(); ++it) {
    if (it->id() == device_name_.id())
      break;
  }
  if (it == device_names.end())
    return false;

  if (AVFoundationGlue::IsAVFoundationSupported()) {
    capture_device_ =
        [[VideoCaptureDeviceAVFoundation alloc] initWithFrameReceiver:this];
  } else {
    capture_device_ =
        [[VideoCaptureDeviceQTKit alloc] initWithFrameReceiver:this];
  }

  if (!capture_device_)
    return false;

  state_ = kIdle;
  return true;
}

void VideoCaptureDeviceMac::ReceiveFrame(
    const uint8* video_frame,
    int video_frame_length,
    const VideoCaptureFormat& frame_format,
    int aspect_numerator,
    int aspect_denominator) {
  // This method is safe to call from a device capture thread, i.e. any thread
  // controlled by QTKit/AVFoundation.
  if (!final_resolution_selected_) {
    DCHECK(!AVFoundationGlue::IsAVFoundationSupported());
    if (capture_format_.frame_size.width() > kVGA.width ||
        capture_format_.frame_size.height() > kVGA.height) {
      // We are requesting HD.  Make sure that the picture is good, otherwise
      // drop down to VGA.
      bool change_to_vga = false;
      if (frame_format.frame_size.width() <
          capture_format_.frame_size.width() ||
          frame_format.frame_size.height() <
          capture_format_.frame_size.height()) {
        // These are the default capture settings, not yet configured to match
        // |capture_format_|.
        DCHECK(frame_format.frame_rate == 0);
        DVLOG(1) << "Switching to VGA because the default resolution is " <<
            frame_format.frame_size.ToString();
        change_to_vga = true;
      }

      if (capture_format_.frame_size == frame_format.frame_size &&
          aspect_numerator != aspect_denominator) {
        DVLOG(1) << "Switching to VGA because HD has nonsquare pixel " <<
            "aspect ratio " << aspect_numerator << ":" << aspect_denominator;
        change_to_vga = true;
      }

      if (change_to_vga)
        capture_format_.frame_size.SetSize(kVGA.width, kVGA.height);
    }

    if (capture_format_.frame_size == frame_format.frame_size &&
        !tried_to_square_pixels_ &&
        (aspect_numerator > kMaxPixelAspectRatio * aspect_denominator ||
         aspect_denominator > kMaxPixelAspectRatio * aspect_numerator)) {
      // The requested size results in non-square PAR. Shrink the frame to 1:1
      // PAR (assuming QTKit selects the same input mode, which is not
      // guaranteed).
      int new_width = capture_format_.frame_size.width();
      int new_height = capture_format_.frame_size.height();
      if (aspect_numerator < aspect_denominator)
        new_width = (new_width * aspect_numerator) / aspect_denominator;
      else
        new_height = (new_height * aspect_denominator) / aspect_numerator;
      capture_format_.frame_size.SetSize(new_width, new_height);
      tried_to_square_pixels_ = true;
    }

    if (capture_format_.frame_size == frame_format.frame_size) {
      final_resolution_selected_ = true;
    } else {
      UpdateCaptureResolution();
      // Let the resolution update sink through QTKit and wait for next frame.
      return;
    }
  }

  // QTKit capture source can change resolution if someone else reconfigures the
  // camera, and that is fine: http://crbug.com/353620. In AVFoundation, this
  // should not happen, it should resize internally.
  if (!AVFoundationGlue::IsAVFoundationSupported()) {
    capture_format_.frame_size = frame_format.frame_size;
  } else if (capture_format_.frame_size != frame_format.frame_size) {
    ReceiveError("Captured resolution " + frame_format.frame_size.ToString() +
        ", and expected " + capture_format_.frame_size.ToString());
    return;
  }

  client_->OnIncomingCapturedData(video_frame,
                                  video_frame_length,
                                  capture_format_,
                                  0,
                                  base::TimeTicks::Now());
}

void VideoCaptureDeviceMac::ReceiveError(const std::string& reason) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&VideoCaptureDeviceMac::SetErrorState,
                                    weak_factory_.GetWeakPtr(),
                                    reason));
}

void VideoCaptureDeviceMac::SetErrorState(const std::string& reason) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DLOG(ERROR) << reason;
  state_ = kError;
  client_->OnError(reason);
}

bool VideoCaptureDeviceMac::UpdateCaptureResolution() {
 if (![capture_device_ setCaptureHeight:capture_format_.frame_size.height()
                                  width:capture_format_.frame_size.width()
                              frameRate:capture_format_.frame_rate]) {
   ReceiveError("Could not configure capture device.");
   return false;
 }
 return true;
}

} // namespace media
