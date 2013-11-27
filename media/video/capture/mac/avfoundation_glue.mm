// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/video/capture/mac/avfoundation_glue.h"

#include <dlfcn.h>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/mac/mac_util.h"
#include "media/base/media_switches.h"

namespace {

// This class is used to retrieve AVFoundation NSBundle and library handle. It
// must be used as a LazyInstance so that it is initialised once and in a
// thread-safe way. Normally no work is done in constructors: LazyInstance is
// an exception.
class AVFoundationInternal {
 public:
  AVFoundationInternal() {
    bundle_ = [NSBundle
        bundleWithPath:@"/System/Library/Frameworks/AVFoundation.framework"];

    const char* path = [[bundle_ executablePath] fileSystemRepresentation];
    CHECK(path);
    library_handle_ = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    CHECK(library_handle_) << dlerror();
  }
  NSBundle* bundle() const { return bundle_; }
  void* library_handle() const { return library_handle_; }

 private:
  NSBundle* bundle_;
  void* library_handle_;

  DISALLOW_COPY_AND_ASSIGN(AVFoundationInternal);
};

}  // namespace

static base::LazyInstance<AVFoundationInternal> g_avfoundation_handle =
    LAZY_INSTANCE_INITIALIZER;

namespace media {

// TODO(mcasas):http://crbug.com/323536 cache the string pointers.
static NSString* ReadNSStringPtr(const char* symbol) {
  NSString** string_pointer = reinterpret_cast<NSString**>(
      dlsym(AVFoundationGlue::AVFoundationLibraryHandle(), symbol));
  DCHECK(string_pointer) << dlerror();
  return *string_pointer;
}

}  // namespace media

bool AVFoundationGlue::IsAVFoundationSupported() {
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  return cmd_line->HasSwitch(switches::kEnableAVFoundation) &&
      base::mac::IsOSLionOrLater() && [AVFoundationBundle() load];
}

NSBundle const* AVFoundationGlue::AVFoundationBundle() {
  return g_avfoundation_handle.Get().bundle();
}

void* AVFoundationGlue::AVFoundationLibraryHandle() {
  return g_avfoundation_handle.Get().library_handle();
}

NSString* AVFoundationGlue::AVCaptureDeviceWasConnectedNotification() {
  return media::ReadNSStringPtr("AVCaptureDeviceWasConnectedNotification");
}

NSString* AVFoundationGlue::AVCaptureDeviceWasDisconnectedNotification() {
  return media::ReadNSStringPtr("AVCaptureDeviceWasDisconnectedNotification");
}

NSString* AVFoundationGlue::AVMediaTypeVideo() {
  return media::ReadNSStringPtr("AVMediaTypeVideo");
}

NSString* AVFoundationGlue::AVMediaTypeAudio() {
  return media::ReadNSStringPtr("AVMediaTypeAudio");
}

NSString* AVFoundationGlue::AVMediaTypeMuxed() {
  return media::ReadNSStringPtr("AVMediaTypeMuxed");
}

NSString* AVFoundationGlue::AVCaptureSessionRuntimeErrorNotification() {
  return media::ReadNSStringPtr("AVCaptureSessionRuntimeErrorNotification");
}

NSString* AVFoundationGlue::AVCaptureSessionDidStopRunningNotification() {
  return media::ReadNSStringPtr("AVCaptureSessionDidStopRunningNotification");
}

NSString* AVFoundationGlue::AVCaptureSessionErrorKey() {
  return media::ReadNSStringPtr("AVCaptureSessionErrorKey");
}

NSString* AVFoundationGlue::AVCaptureSessionPreset320x240() {
  return media::ReadNSStringPtr("AVCaptureSessionPreset320x240");
}

NSString* AVFoundationGlue::AVCaptureSessionPreset640x480() {
  return media::ReadNSStringPtr("AVCaptureSessionPreset640x480");
}

NSString* AVFoundationGlue::AVCaptureSessionPreset1280x720() {
  return media::ReadNSStringPtr("AVCaptureSessionPreset1280x720");
}

NSString* AVFoundationGlue::AVVideoScalingModeKey() {
  return media::ReadNSStringPtr("AVVideoScalingModeKey");
}

NSString* AVFoundationGlue::AVVideoScalingModeResizeAspect() {
  return media::ReadNSStringPtr("AVVideoScalingModeResizeAspect");
}

Class AVFoundationGlue::AVCaptureSessionClass() {
  return [AVFoundationBundle() classNamed:@"AVCaptureSession"];
}

Class AVFoundationGlue::AVCaptureVideoDataOutputClass() {
  return [AVFoundationBundle() classNamed:@"AVCaptureVideoDataOutput"];
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

+ (CrAVCaptureDevice*)deviceWithUniqueID:(NSString*)deviceUniqueID {
  Class avcClass =
      [AVFoundationGlue::AVFoundationBundle() classNamed:@"AVCaptureDevice"];
  return [avcClass performSelector:@selector(deviceWithUniqueID:)
                        withObject:deviceUniqueID];
}

@end  // @implementation AVCaptureDeviceGlue

@implementation AVCaptureDeviceInputGlue

+ (CrAVCaptureDeviceInput*)deviceInputWithDevice:(CrAVCaptureDevice*)device
                                           error:(NSError**)outError {
  return [[AVFoundationGlue::AVFoundationBundle()
      classNamed:@"AVCaptureDeviceInput"] deviceInputWithDevice:device
                                                          error:outError];
}

@end  // @implementation AVCaptureDeviceInputGlue
