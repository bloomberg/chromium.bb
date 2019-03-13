// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Authorization functions and types are available on 10.14+.
// To avoid availability compile errors, use performSelector invocation of
// functions, NSInteger instead of AVAuthorizationStatus, and NSString* instead
// of AVMediaType.
// The AVAuthorizationStatus enum is defined as follows (10.14 SDK):
// AVAuthorizationStatusNotDetermined = 0,
// AVAuthorizationStatusRestricted    = 1,
// AVAuthorizationStatusDenied        = 2,
// AVAuthorizationStatusAuthorized    = 3,
// TODO(grunell): Call functions directly and use AVAuthorizationStatus once
// we use the 10.14 SDK.

#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"

#import <AVFoundation/AVFoundation.h>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

NSInteger MediaAuthorizationStatus(NSString* media_type) {
  if (@available(macOS 10.14, *)) {
    AVCaptureDevice* target = [AVCaptureDevice class];
    SEL selector = @selector(authorizationStatusForMediaType:);
    NSInteger auth_status = 0;
    if ([target respondsToSelector:selector]) {
      auth_status =
          (NSInteger)[target performSelector:selector withObject:media_type];
    } else {
      DLOG(WARNING) << "authorizationStatusForMediaType could not be executed";
    }
    return auth_status;
  }

  NOTREACHED();
  return 0;
}

SystemPermission CheckSystemMediaCapturePermission(NSString* media_type) {
  if (@available(macOS 10.14, *)) {
    NSInteger auth_status = MediaAuthorizationStatus(media_type);
    switch (auth_status) {
      case 0:
        return SystemPermission::kNotDetermined;
      case 1:
      case 2:
        return SystemPermission::kNotAllowed;
      case 3:
        return SystemPermission::kAllowed;
      default:
        NOTREACHED();
        return SystemPermission::kAllowed;
    }
  }

  // On pre-10.14, there are no system permissions, so we return allowed.
  return SystemPermission::kAllowed;
}

// Use RepeatingCallback since it must be copyable for use in the block. It's
// only called once though.
void RequestSystemMediaCapturePermission(NSString* media_type,
                                         base::RepeatingClosure callback,
                                         const base::TaskTraits& traits) {
  if (@available(macOS 10.14, *)) {
      AVCaptureDevice* target = [AVCaptureDevice class];
      SEL selector = @selector(requestAccessForMediaType:completionHandler:);
      if ([target respondsToSelector:selector]) {
        [target performSelector:selector
                     withObject:media_type
                     withObject:^(BOOL granted) {
                       base::PostTaskWithTraits(FROM_HERE, traits,
                                                std::move(callback));
                     }];
      } else {
        DLOG(WARNING) << "requestAccessForMediaType could not be executed";
      }
  } else {
    NOTREACHED();
    // Should never happen since for pre-10.14 system permissions don't exist
    // and checking them in CheckSystemAudioCapturePermission() will always
    // return allowed, and this function should not be called.
    base::PostTaskWithTraits(FROM_HERE, traits, std::move(callback));
  }
}

}  // namespace

SystemPermission CheckSystemAudioCapturePermission() {
  return CheckSystemMediaCapturePermission(AVMediaTypeAudio);
}

SystemPermission CheckSystemVideoCapturePermission() {
  return CheckSystemMediaCapturePermission(AVMediaTypeVideo);
}

void RequestSystemAudioCapturePermisson(base::OnceClosure callback,
                                        const base::TaskTraits& traits) {
  RequestSystemMediaCapturePermission(
      AVMediaTypeAudio, base::AdaptCallbackForRepeating(std::move(callback)),
      traits);
}

void RequestSystemVideoCapturePermisson(base::OnceClosure callback,
                                        const base::TaskTraits& traits) {
  RequestSystemMediaCapturePermission(
      AVMediaTypeVideo, base::AdaptCallbackForRepeating(std::move(callback)),
      traits);
}
