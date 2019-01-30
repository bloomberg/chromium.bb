// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Authorization functions and types are available on 10.14+.
// To avoid availability compile errors, use performSelector invocation of
// functions and NSInteger instead of AVAuthorizationStatus.
// The AVAuthorizationStatus enum is defined as follows (10.14 SDK):
// AVAuthorizationStatusNotDetermined = 0,
// AVAuthorizationStatusRestricted    = 1,
// AVAuthorizationStatusDenied        = 2,
// AVAuthorizationStatusAuthorized    = 3,
// TODO(grunell): Call functions directly and use AVAuthorizationStatus once
// we use the 10.14 SDK.

#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"

#import <AVFoundation/AVFoundation.h>

#include "base/logging.h"

namespace {

NSInteger AudioAuthorizationStatus() {
  if (@available(macOS 10.14, *)) {
    AVCaptureDevice* target = [AVCaptureDevice class];
    SEL selector = @selector(authorizationStatusForMediaType:);
    NSInteger auth_status = 0;
    if ([target respondsToSelector:selector]) {
      auth_status = (NSInteger)[target performSelector:selector
                                            withObject:AVMediaTypeAudio];
    } else {
      DLOG(WARNING) << "authorizationStatusForMediaType could not be executed";
    }
    return auth_status;
  }

  NOTREACHED();
  return 0;
}

}  // namespace

bool SystemAudioCapturePermissionIsDisallowed() {
  if (@available(macOS 10.14, *)) {
    NSInteger auth_status = AudioAuthorizationStatus();
    return auth_status == 1 || auth_status == 2;
  }
  return false;
}

void EnsureSystemAudioCapturePermission() {
  if (@available(macOS 10.14, *)) {
    if (AudioAuthorizationStatus() == 0) {
      AVCaptureDevice* target = [AVCaptureDevice class];
      SEL selector = @selector(requestAccessForMediaType:completionHandler:);
      if ([target respondsToSelector:selector]) {
        [target performSelector:selector
                     withObject:AVMediaTypeAudio
                     withObject:^(BOOL granted){
                     }];
      } else {
        DLOG(WARNING) << "requestAccessForMediaType could not be executed";
      }
    }
  }
}
