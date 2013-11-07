// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AVFoundation API is only introduced in Mac OS X > 10.6, and there is only one
// build of Chromium, so the (potential) linking with AVFoundation has to happen
// in runtime. For this to be clean, a AVFoundationGlue class is defined to try
// and load these AVFoundation system libraries. If it succeeds, subsequent
// clients can use AVFoundation via the rest of the classes declared in this
// file.

#ifndef MEDIA_VIDEO_CAPTURE_MAC_AVFOUNDATION_GLUE_H_
#define MEDIA_VIDEO_CAPTURE_MAC_AVFOUNDATION_GLUE_H_

#import <Foundation/Foundation.h>

#include "base/basictypes.h"
#include "media/base/media_export.h"

class MEDIA_EXPORT AVFoundationGlue {
 public:
  // This method returns true if the OS version supports AVFoundation and the
  // AVFoundation bundle could be loaded correctly, or false otherwise.
  static bool IsAVFoundationSupported();

  static NSBundle const* AVFoundationBundle();

  static void* AVFoundationLibraryHandle();

  // Originally coming from AVCaptureDevice.h but in global namespace.
  static NSString* AVCaptureDeviceWasConnectedNotification();
  static NSString* AVCaptureDeviceWasDisconnectedNotification();

  // Originally coming from AVMediaFormat.h but in global namespace.
  static NSString* AVMediaTypeVideo();
  static NSString* AVMediaTypeAudio();
  static NSString* AVMediaTypeMuxed();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AVFoundationGlue);
};

// Originally AVCaptureDevice and coming from AVCaptureDevice.h
MEDIA_EXPORT
@interface CrAVCaptureDevice : NSObject

- (BOOL)hasMediaType:(NSString*)mediaType;

- (NSString*)uniqueID;

@end

MEDIA_EXPORT
@interface AVCaptureDeviceGlue : NSObject

+ (NSArray*)devices;

@end

#endif  // MEDIA_VIDEO_CAPTURE_MAC_AVFOUNDATION_GLUE_H_
