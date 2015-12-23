// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MacOSX implementation of generic VideoCaptureDevice, using either QTKit or
// AVFoundation as native capture API. QTKit is available in all OSX versions,
// although namely deprecated in 10.9, and AVFoundation is available in versions
// 10.7 (Lion) and later.

#ifndef MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_
#define MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_

#import <Foundation/Foundation.h>
#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/video_capture_types.h"
#include "media/capture/video/video_capture_device.h"

@protocol PlatformVideoCapturingMac;

namespace base {
class SingleThreadTaskRunner;
}

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

// Small class to bundle device name and connection type into a dictionary.
MEDIA_EXPORT
@interface DeviceNameAndTransportType : NSObject {
 @private
  base::scoped_nsobject<NSString> deviceName_;
  // The transport type of the device (USB, PCI, etc), values are defined in
  // <IOKit/audio/IOAudioTypes.h> as kIOAudioDeviceTransportType*.
  int32_t transportType_;
}

- (id)initWithName:(NSString*)name transportType:(int32_t)transportType;

- (NSString*)deviceName;
- (int32_t)transportType;
@end

namespace media {

enum {
  // Unknown transport type, addition to the kIOAudioDeviceTransportType*
  // family for QTKit devices where this attribute isn't published.
  kIOAudioDeviceTransportTypeUnknown = 'unkn'
};

// Called by VideoCaptureManager to open, close and start, stop Mac video
// capture devices.
class VideoCaptureDeviceMac : public VideoCaptureDevice {
 public:
  explicit VideoCaptureDeviceMac(const Name& device_name);
  ~VideoCaptureDeviceMac() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        scoped_ptr<VideoCaptureDevice::Client> client) override;
  void StopAndDeAllocate() override;

  bool Init(VideoCaptureDevice::Name::CaptureApiType capture_api_type);

  // Called to deliver captured video frames.
  void ReceiveFrame(const uint8_t* video_frame,
                    int video_frame_length,
                    const VideoCaptureFormat& frame_format,
                    int aspect_numerator,
                    int aspect_denominator,
                    base::TimeDelta timestamp);

  // Forwarder to VideoCaptureDevice::Client::OnError().
  void ReceiveError(const tracked_objects::Location& from_here,
                    const std::string& reason);

  // Forwarder to VideoCaptureDevice::Client::OnLog().
  void LogMessage(const std::string& message);

 private:
  void SetErrorState(const tracked_objects::Location& from_here,
                     const std::string& reason);
  bool UpdateCaptureResolution();

  // Flag indicating the internal state.
  enum InternalState { kNotInitialized, kIdle, kCapturing, kError };

  Name device_name_;
  scoped_ptr<VideoCaptureDevice::Client> client_;

  VideoCaptureFormat capture_format_;
  // These variables control the two-step configure-start process for QTKit HD:
  // the device is first started with no configuration and the captured frames
  // are inspected to check if the camera really supports HD. AVFoundation does
  // not need this process so |final_resolution_selected_| is false then.
  bool final_resolution_selected_;
  bool tried_to_square_pixels_;

  // Only read and write state_ from inside this loop.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  InternalState state_;

  id<PlatformVideoCapturingMac> capture_device_;

  base::TimeDelta first_timestamp_;
  base::TimeTicks first_aligned_timestamp_;

  // Used with Bind and PostTask to ensure that methods aren't called after the
  // VideoCaptureDeviceMac is destroyed.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoCaptureDeviceMac> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceMac);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_
