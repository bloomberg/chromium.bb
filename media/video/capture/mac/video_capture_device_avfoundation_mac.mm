// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/video/capture/mac/video_capture_device_avfoundation_mac.h"

#include "base/logging.h"
#import "media/video/capture/mac/avfoundation_glue.h"

@implementation VideoCaptureDeviceAVFoundation

#pragma mark Class methods

+ (void)getDeviceNames:(NSMutableDictionary*)deviceNames {
  // Calling the +devices method will load NSBundle and create device drivers.
  NSArray* devices = [AVCaptureDeviceGlue devices];
  for (CrAVCaptureDevice* device in devices) {
    if ([device hasMediaType:AVFoundationGlue::AVMediaTypeVideo()] ||
        [device hasMediaType:AVFoundationGlue::AVMediaTypeMuxed()]) {
      [deviceNames setObject:[device localizedName]
                      forKey:[device uniqueID]];
    }
  }
}

+ (NSDictionary*)deviceNames {
  NSMutableDictionary* deviceNames =
      [[[NSMutableDictionary alloc] init] autorelease];
  // The device name retrieval is not going to happen in the main thread, and
  // this might cause instabilities (it did in QTKit), so keep an eye here.
  [self getDeviceNames:deviceNames];
  return deviceNames;
}

#pragma mark Public methods

- (id)initWithFrameReceiver:(media::VideoCaptureDeviceMac*)frameReceiver {
  NOTIMPLEMENTED();
  return nil;
}

- (void)setFrameReceiver:(media::VideoCaptureDeviceMac*)frameReceiver {
  NOTIMPLEMENTED();
}

- (BOOL)setCaptureDevice:(NSString*)deviceId {
  NOTIMPLEMENTED();
  return NO;
}

- (BOOL)setCaptureHeight:(int)height width:(int)width frameRate:(int)frameRate {
  NOTIMPLEMENTED();
  return NO;
}

- (BOOL)startCapture {
  NOTIMPLEMENTED();
  return NO;
}

- (void)stopCapture {
  NOTIMPLEMENTED();
}

@end
