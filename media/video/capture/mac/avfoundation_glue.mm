// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/video/capture/mac/avfoundation_glue.h"

#include <dlfcn.h>

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "media/base/media_switches.h"

namespace {

NSString* ReadNSStringPtr(const char* symbol) {
  NSString** string_pointer = reinterpret_cast<NSString**>(
      dlsym(AVFoundationGlue::AVFoundationLibraryHandle(), symbol));
  DCHECK(string_pointer) << dlerror();
  return *string_pointer;
}

}  // namespace


bool AVFoundationGlue::IsAVFoundationSupported() {
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  return cmd_line->HasSwitch(switches::kEnableAVFoundation) &&
      base::mac::IsOSLionOrLater() && [AVFoundationBundle() load];
}

NSBundle const* AVFoundationGlue::AVFoundationBundle() {
  static NSBundle* bundle = [NSBundle
      bundleWithPath:@"/System/Library/Frameworks/AVFoundation.framework"];
  return bundle;
}

void* AVFoundationGlue::AVFoundationLibraryHandle() {
  const char* library_path =
      [[AVFoundationBundle() executablePath] fileSystemRepresentation];
  if (library_path == NULL) {
    DCHECK(false);
    return NULL;
  }
  static void* library_handle = dlopen(library_path, RTLD_LAZY | RTLD_LOCAL);
  DCHECK(library_handle) << dlerror();
  return library_handle;
}

NSString* AVFoundationGlue::AVCaptureDeviceWasConnectedNotification() {
  return ReadNSStringPtr("AVCaptureDeviceWasConnectedNotification");
}

NSString* AVFoundationGlue::AVCaptureDeviceWasDisconnectedNotification() {
  return ReadNSStringPtr("AVCaptureDeviceWasDisconnectedNotification");
}

NSString* AVFoundationGlue::AVMediaTypeVideo() {
  return ReadNSStringPtr("AVMediaTypeVideo");
}

NSString* AVFoundationGlue::AVMediaTypeAudio() {
  return ReadNSStringPtr("AVMediaTypeAudio");
}

NSString* AVFoundationGlue::AVMediaTypeMuxed() {
  return ReadNSStringPtr("AVMediaTypeMuxed");
}

@implementation AVCaptureDeviceGlue

+ (NSArray*)devices {
  Class avcClass =
      [AVFoundationGlue::AVFoundationBundle() classNamed:@"AVCaptureDevice"];
  if ([avcClass respondsToSelector:@selector(devices)]) {
    return [avcClass performSelector:@selector(devices)];
  }
  return nil;
}

@end  // @implementation AVCaptureDeviceGlue
