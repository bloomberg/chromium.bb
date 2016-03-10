// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/base/mac/avfoundation_glue.h"

#import <AVFoundation/AVFoundation.h>
#include <dlfcn.h>
#include <stddef.h>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/mac/mac_util.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_switches.h"

// Forward declarations of AVFoundation.h strings.
// This is needed to avoid compile time warnings since currently
// |mac_deployment_target| is 10.6.
extern NSString* const AVCaptureDeviceWasConnectedNotification;
extern NSString* const AVCaptureDeviceWasDisconnectedNotification;
extern NSString* const AVMediaTypeVideo;
extern NSString* const AVMediaTypeAudio;
extern NSString* const AVMediaTypeMuxed;
extern NSString* const AVCaptureSessionRuntimeErrorNotification;
extern NSString* const AVCaptureSessionDidStopRunningNotification;
extern NSString* const AVCaptureSessionErrorKey;
extern NSString* const AVVideoScalingModeKey;
extern NSString* const AVVideoScalingModeResizeAspectFill;

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
  NSString* AVCaptureDeviceWasConnectedNotification_ =
      ::AVCaptureDeviceWasConnectedNotification;
  NSString* AVCaptureDeviceWasDisconnectedNotification_ =
      ::AVCaptureDeviceWasDisconnectedNotification;
  NSString* AVMediaTypeVideo_ = ::AVMediaTypeVideo;
  NSString* AVMediaTypeAudio_ = ::AVMediaTypeAudio;
  NSString* AVMediaTypeMuxed_ = ::AVMediaTypeMuxed;
  NSString* AVCaptureSessionRuntimeErrorNotification_ =
      ::AVCaptureSessionRuntimeErrorNotification;
  NSString* AVCaptureSessionDidStopRunningNotification_ =
      ::AVCaptureSessionDidStopRunningNotification;
  NSString* AVCaptureSessionErrorKey_ = ::AVCaptureSessionErrorKey;
  NSString* AVVideoScalingModeKey_ = ::AVVideoScalingModeKey;
  NSString* AVVideoScalingModeResizeAspectFill_ =
      ::AVVideoScalingModeResizeAspectFill;

  DISALLOW_COPY_AND_ASSIGN(AVFoundationInternal);
};

static base::ThreadLocalStorage::StaticSlot g_avfoundation_handle =
    TLS_INITIALIZER;

void TlsCleanup(void* value) {
  delete static_cast<AVFoundationInternal*>(value);
}

AVFoundationInternal* GetAVFoundationInternal() {
  return static_cast<AVFoundationInternal*>(g_avfoundation_handle.Get());
}

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
  g_avfoundation_handle.Initialize(TlsCleanup);
  g_avfoundation_handle.Set(new AVFoundationInternal());
  const bool ret = [AVFoundationGlue::AVFoundationBundle() load];
  LogCaptureApi(ret ? CAPTURE_API_AVFOUNDATION_LOADED_OK
                    : CAPTURE_API_QTKIT_DUE_TO_AVFOUNDATION_LOAD_ERROR);

  return ret;
}

enum {
  INITIALIZE_NOT_CALLED = 0,
  AVFOUNDATION_IS_SUPPORTED,
  AVFOUNDATION_NOT_SUPPORTED
} static g_avfoundation_initialization = INITIALIZE_NOT_CALLED;

}  // namespace

void AVFoundationGlue::InitializeAVFoundation() {
  TRACE_EVENT0("video", "AVFoundationGlue::InitializeAVFoundation");
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
  return GetAVFoundationInternal()->bundle();
}

NSString* AVFoundationGlue::AVCaptureDeviceWasConnectedNotification() {
  return GetAVFoundationInternal()->AVCaptureDeviceWasConnectedNotification();
}

NSString* AVFoundationGlue::AVCaptureDeviceWasDisconnectedNotification() {
  return GetAVFoundationInternal()
      ->AVCaptureDeviceWasDisconnectedNotification();
}

NSString* AVFoundationGlue::AVMediaTypeVideo() {
  return GetAVFoundationInternal()->AVMediaTypeVideo();
}

NSString* AVFoundationGlue::AVMediaTypeAudio() {
  return GetAVFoundationInternal()->AVMediaTypeAudio();
}

NSString* AVFoundationGlue::AVMediaTypeMuxed() {
  return GetAVFoundationInternal()->AVMediaTypeMuxed();
}

NSString* AVFoundationGlue::AVCaptureSessionRuntimeErrorNotification() {
  return GetAVFoundationInternal()->AVCaptureSessionRuntimeErrorNotification();
}

NSString* AVFoundationGlue::AVCaptureSessionDidStopRunningNotification() {
  return GetAVFoundationInternal()
      ->AVCaptureSessionDidStopRunningNotification();
}

NSString* AVFoundationGlue::AVCaptureSessionErrorKey() {
  return GetAVFoundationInternal()->AVCaptureSessionErrorKey();
}

NSString* AVFoundationGlue::AVVideoScalingModeKey() {
  return GetAVFoundationInternal()->AVVideoScalingModeKey();
}

NSString* AVFoundationGlue::AVVideoScalingModeResizeAspectFill() {
  return GetAVFoundationInternal()->AVVideoScalingModeResizeAspectFill();
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
