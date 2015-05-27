// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/base/mac/avfoundation_glue.h"

#include <dlfcn.h>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/mac/mac_util.h"
#include "base/metrics/histogram.h"
#include "media/base/media_switches.h"

namespace {

// Used for logging capture API usage. Classes are a partition. Elements in this
// enum should not be deleted or rearranged; the only permitted operation is to
// add new elements before CAPTURE_API_MAX, that must be equal to the last item.
enum CaptureApi {
  CAPTURE_API_QTKIT_DUE_TO_OS_PREVIOUS_TO_LION = 0,
  CAPTURE_API_QTKIT_FORCED_BY_FLAG = 1,
  CAPTURE_API_QTKIT_DUE_TO_NO_FLAG = 2,
  CAPTURE_API_QTKIT_DUE_TO_AVFOUNDATION_LOAD_ERROR = 3,
  CAPTURE_API_AVFOUNDATION_LOADED_OK = 4,
  CAPTURE_API_MAX = CAPTURE_API_AVFOUNDATION_LOADED_OK
};

void LogCaptureApi(CaptureApi api) {
  UMA_HISTOGRAM_ENUMERATION("Media.VideoCaptureApi.Mac",
                            api,
                            CAPTURE_API_MAX + 1);
}

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

    struct {
      NSString** loaded_string;
      const char* symbol;
    } av_strings[] = {
        {&AVCaptureDeviceWasConnectedNotification_,
         "AVCaptureDeviceWasConnectedNotification"},
        {&AVCaptureDeviceWasDisconnectedNotification_,
         "AVCaptureDeviceWasDisconnectedNotification"},
        {&AVMediaTypeVideo_, "AVMediaTypeVideo"},
        {&AVMediaTypeAudio_, "AVMediaTypeAudio"},
        {&AVMediaTypeMuxed_, "AVMediaTypeMuxed"},
        {&AVCaptureSessionRuntimeErrorNotification_,
         "AVCaptureSessionRuntimeErrorNotification"},
        {&AVCaptureSessionDidStopRunningNotification_,
         "AVCaptureSessionDidStopRunningNotification"},
        {&AVCaptureSessionErrorKey_, "AVCaptureSessionErrorKey"},
        {&AVVideoScalingModeKey_, "AVVideoScalingModeKey"},
        {&AVVideoScalingModeResizeAspectFill_,
         "AVVideoScalingModeResizeAspectFill"},
    };
    for (size_t i = 0; i < arraysize(av_strings); ++i) {
      *av_strings[i].loaded_string = *reinterpret_cast<NSString**>(
          dlsym(library_handle_, av_strings[i].symbol));
      DCHECK(*av_strings[i].loaded_string) << dlerror();
    }
  }

  NSBundle* bundle() const { return bundle_; }

  NSString* AVCaptureDeviceWasConnectedNotification() const {
    return AVCaptureDeviceWasConnectedNotification_;
  }
  NSString* AVCaptureDeviceWasDisconnectedNotification() const {
    return AVCaptureDeviceWasDisconnectedNotification_;
  }
  NSString* AVMediaTypeVideo() const { return AVMediaTypeVideo_; }
  NSString* AVMediaTypeAudio() const { return AVMediaTypeAudio_; }
  NSString* AVMediaTypeMuxed() const { return AVMediaTypeMuxed_; }
  NSString* AVCaptureSessionRuntimeErrorNotification() const {
    return AVCaptureSessionRuntimeErrorNotification_;
  }
  NSString* AVCaptureSessionDidStopRunningNotification() const {
    return AVCaptureSessionDidStopRunningNotification_;
  }
  NSString* AVCaptureSessionErrorKey() const {
    return AVCaptureSessionErrorKey_;
  }
  NSString* AVVideoScalingModeKey() const { return AVVideoScalingModeKey_; }
  NSString* AVVideoScalingModeResizeAspectFill() const {
    return AVVideoScalingModeResizeAspectFill_;
  }

 private:
  NSBundle* bundle_;
  void* library_handle_;
  // The following members are replicas of the respectives in AVFoundation.
  NSString* AVCaptureDeviceWasConnectedNotification_;
  NSString* AVCaptureDeviceWasDisconnectedNotification_;
  NSString* AVMediaTypeVideo_;
  NSString* AVMediaTypeAudio_;
  NSString* AVMediaTypeMuxed_;
  NSString* AVCaptureSessionRuntimeErrorNotification_;
  NSString* AVCaptureSessionDidStopRunningNotification_;
  NSString* AVCaptureSessionErrorKey_;
  NSString* AVVideoScalingModeKey_;
  NSString* AVVideoScalingModeResizeAspectFill_;

  DISALLOW_COPY_AND_ASSIGN(AVFoundationInternal);
};

// This contains the logic of checking whether AVFoundation is supported.
// It's called only once and the results are cached in a static bool.
bool LoadAVFoundationInternal() {
  // AVFoundation is only available on OS Lion and above.
  if (!base::mac::IsOSLionOrLater()) {
    LogCaptureApi(CAPTURE_API_QTKIT_DUE_TO_OS_PREVIOUS_TO_LION);
    return false;
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  // The force-qtkit flag takes precedence over enable-avfoundation.
  if (command_line->HasSwitch(switches::kForceQTKit)) {
    LogCaptureApi(CAPTURE_API_QTKIT_FORCED_BY_FLAG);
    return false;
  }

  if (!command_line->HasSwitch(switches::kEnableAVFoundation)) {
    LogCaptureApi(CAPTURE_API_QTKIT_DUE_TO_NO_FLAG);
    return false;
  }
  const bool ret = [AVFoundationGlue::AVFoundationBundle() load];
  LogCaptureApi(ret ? CAPTURE_API_AVFOUNDATION_LOADED_OK
                    : CAPTURE_API_QTKIT_DUE_TO_AVFOUNDATION_LOAD_ERROR);
  return ret;
}

}  // namespace

static base::LazyInstance<AVFoundationInternal>::Leaky g_avfoundation_handle =
    LAZY_INSTANCE_INITIALIZER;

enum {
  INITIALIZE_NOT_CALLED = 0,
  AVFOUNDATION_IS_SUPPORTED,
  AVFOUNDATION_NOT_SUPPORTED
} static g_avfoundation_initialization = INITIALIZE_NOT_CALLED;

void AVFoundationGlue::InitializeAVFoundation() {
  CHECK([NSThread isMainThread]);
  if (g_avfoundation_initialization != INITIALIZE_NOT_CALLED)
    return;
  g_avfoundation_initialization = LoadAVFoundationInternal() ?
      AVFOUNDATION_IS_SUPPORTED : AVFOUNDATION_NOT_SUPPORTED;
}

bool AVFoundationGlue::IsAVFoundationSupported() {
  CHECK_NE(g_avfoundation_initialization, INITIALIZE_NOT_CALLED);
  return g_avfoundation_initialization == AVFOUNDATION_IS_SUPPORTED;
}

NSBundle const* AVFoundationGlue::AVFoundationBundle() {
  return g_avfoundation_handle.Get().bundle();
}

NSString* AVFoundationGlue::AVCaptureDeviceWasConnectedNotification() {
  return g_avfoundation_handle.Get().AVCaptureDeviceWasConnectedNotification();
}

NSString* AVFoundationGlue::AVCaptureDeviceWasDisconnectedNotification() {
  return
      g_avfoundation_handle.Get().AVCaptureDeviceWasDisconnectedNotification();
}

NSString* AVFoundationGlue::AVMediaTypeVideo() {
  return g_avfoundation_handle.Get().AVMediaTypeVideo();
}

NSString* AVFoundationGlue::AVMediaTypeAudio() {
  return g_avfoundation_handle.Get().AVMediaTypeAudio();
}

NSString* AVFoundationGlue::AVMediaTypeMuxed() {
  return g_avfoundation_handle.Get().AVMediaTypeMuxed();
}

NSString* AVFoundationGlue::AVCaptureSessionRuntimeErrorNotification() {
  return g_avfoundation_handle.Get().AVCaptureSessionRuntimeErrorNotification();
}

NSString* AVFoundationGlue::AVCaptureSessionDidStopRunningNotification() {
  return
      g_avfoundation_handle.Get().AVCaptureSessionDidStopRunningNotification();
}

NSString* AVFoundationGlue::AVCaptureSessionErrorKey() {
  return g_avfoundation_handle.Get().AVCaptureSessionErrorKey();
}

NSString* AVFoundationGlue::AVVideoScalingModeKey() {
  return g_avfoundation_handle.Get().AVVideoScalingModeKey();
}

NSString* AVFoundationGlue::AVVideoScalingModeResizeAspectFill() {
  return g_avfoundation_handle.Get().AVVideoScalingModeResizeAspectFill();
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
