// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/custom_frame_view.h"

#import <Carbon/Carbon.h>
#include <crt_externs.h>
#import <objc/runtime.h>
#include <string.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"

@interface NSView (Swizzles)
- (NSPoint)_fullScreenButtonOriginOriginal;
@end

@interface NSWindow (FramedBrowserWindow)
- (NSPoint)fullScreenButtonOriginAdjustment;
@end

@interface CustomFrameView : NSView

// Clang emits a warning if designated initializers don't call the super
// initializer, even if the method raises an exception.
// http://www.crbug.com/479019.
- (id)initWithFrame:(NSRect)frame UNAVAILABLE_ATTRIBUTE;
- (id)initWithCoder:(NSCoder*)coder UNAVAILABLE_ATTRIBUTE;

@end

@implementation CustomFrameView

+ (void)load {
  // Swizzling should only happen in the browser process. Interacting with
  // AppKit will run +[borderViewClass initialize] in the renderer, which
  // may establish Mach IPC with com.apple.windowserver.
  // Note that CommandLine has not been initialized yet, since this is running
  // as a module initializer.
  const char* const* const argv = *_NSGetArgv();
  const int argc = *_NSGetArgc();
  const char kType[] = "--type=";
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (strncmp(arg, kType, strlen(kType)) == 0)
      return;
  }

  // In Yosemite, the fullscreen button replaces the zoom button. We no longer
  // need to swizzle out this AppKit private method.
  if (!base::mac::IsOS10_9())
    return;

  base::mac::ScopedNSAutoreleasePool pool;

  // On 10.8+ the background for textured windows are no longer drawn by
  // NSGrayFrame, and NSThemeFrame is used instead <http://crbug.com/114745>.
  Class borderViewClass = NSClassFromString(@"NSThemeFrame");
  DCHECK(borderViewClass);
  if (!borderViewClass) return;

  // Swizzle the method that sets the origin for the Lion fullscreen button.
  // Do nothing if it cannot be found.
  Method m0 = class_getInstanceMethod([self class],
                               @selector(_fullScreenButtonOrigin));
  if (m0) {
    BOOL didAdd = class_addMethod(borderViewClass,
                                  @selector(_fullScreenButtonOriginOriginal),
                                  method_getImplementation(m0),
                                  method_getTypeEncoding(m0));
    if (didAdd) {
      Method m1 = class_getInstanceMethod(borderViewClass,
                                          @selector(_fullScreenButtonOrigin));
      Method m2 = class_getInstanceMethod(
          borderViewClass, @selector(_fullScreenButtonOriginOriginal));
      if (m1 && m2) {
        method_exchangeImplementations(m1, m2);
      }
    }
  }
}

- (id)initWithFrame:(NSRect)frame {
  // This class is not for instantiating.
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

- (id)initWithCoder:(NSCoder*)coder {
  // This class is not for instantiating.
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

// Override to move the fullscreen button to the left of the profile avatar.
- (NSPoint)_fullScreenButtonOrigin {
  NSWindow* window = [self window];
  NSPoint offset = NSZeroPoint;

  if ([window respondsToSelector:@selector(fullScreenButtonOriginAdjustment)])
    offset = [window fullScreenButtonOriginAdjustment];

  NSPoint origin = [self _fullScreenButtonOriginOriginal];
  origin.x += offset.x;
  origin.y += offset.y;
  return origin;
}

@end
