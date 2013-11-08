// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_
#define MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_

#import <Foundation/Foundation.h>

#import "media/video/capture/mac/platform_video_capturing_mac.h"

namespace media {
class VideoCaptureDeviceMac;
}

// Class used by VideoCaptureDeviceMac for video capture using AVFoundation API.
@interface VideoCaptureDeviceAVFoundation : NSObject<PlatformVideoCapturingMac>

// Returns a dictionary of capture devices with friendly name and unique id.
+ (NSDictionary*)deviceNames;

// Initializes the instance and registers the frame receiver.
- (id)initWithFrameReceiver:(media::VideoCaptureDeviceMac*)frameReceiver;

// Set the frame receiver.
- (void)setFrameReceiver:(media::VideoCaptureDeviceMac*)frameReceiver;

// Sets which capture device to use. Returns YES on sucess, NO otherwise.
- (BOOL)setCaptureDevice:(NSString*)deviceId;

// Configures the capture properties.
- (BOOL)setCaptureHeight:(int)height width:(int)width frameRate:(int)frameRate;

// Start video capturing. Returns YES on sucess, NO otherwise.
- (BOOL)startCapture;

// Stops video capturing.
- (void)stopCapture;

@end

#endif  // MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_
