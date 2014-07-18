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

namespace {
BOOL gCanDrawTitle = NO;
BOOL gCanGetCornerRadius = NO;
}  // namespace

@interface NSView (Swizzles)
- (void)drawRectOriginal:(NSRect)rect;
- (NSPoint)_fullScreenButtonOriginOriginal;
@end

@interface NSWindow (FramedBrowserWindow)
- (NSPoint)fullScreenButtonOriginAdjustment;
@end

@implementation NSWindow (CustomFrameView)
- (void)drawCustomFrameRect:(NSRect)rect forView:(NSView*)view {
  [view drawRectOriginal:rect];
}
@end

@interface CustomFrameView : NSView

@end

@implementation CustomFrameView

// This is where we swizzle drawRect, and add in two methods that we
// need. If any of these fail it shouldn't affect the functionality of the
// others. If they all fail, we will lose window frame theming and
// roll overs for our close widgets, but things should still function
// correctly.
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

  base::mac::ScopedNSAutoreleasePool pool;

  // On 10.8+ the background for textured windows are no longer drawn by
  // NSGrayFrame, and NSThemeFrame is used instead <http://crbug.com/114745>.
  Class borderViewClass = NSClassFromString(
      base::mac::IsOSMountainLionOrLater() ? @"NSThemeFrame" : @"NSGrayFrame");
  DCHECK(borderViewClass);
  if (!borderViewClass) return;

  // Exchange draw rect.
  Method m0 = class_getInstanceMethod([self class], @selector(drawRect:));
  DCHECK(m0);
  if (m0) {
    BOOL didAdd = class_addMethod(borderViewClass,
                                  @selector(drawRectOriginal:),
                                  method_getImplementation(m0),
                                  method_getTypeEncoding(m0));
    DCHECK(didAdd);
    if (didAdd) {
      Method m1 = class_getInstanceMethod(borderViewClass,
                                          @selector(drawRect:));
      Method m2 = class_getInstanceMethod(borderViewClass,
                                          @selector(drawRectOriginal:));
      DCHECK(m1 && m2);
      if (m1 && m2) {
        method_exchangeImplementations(m1, m2);
      }
    }
  }

  // In Yosemite, the fullscreen button replaces the zoom button. We no longer
  // need to swizzle out this AppKit private method.
  if (base::mac::IsOSMavericksOrEarlier()) {
    // Swizzle the method that sets the origin for the Lion fullscreen button.
    // Do nothing if it cannot be found.
    m0 = class_getInstanceMethod([self class],
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
}

+ (BOOL)canDrawTitle {
  return gCanDrawTitle;
}

+ (BOOL)canGetCornerRadius {
  return gCanGetCornerRadius;
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

// Here is our custom drawing for our frame.
- (void)drawRect:(NSRect)rect {
  // Delegate drawing to the window, whose default implementation (above) is to
  // call into the original implementation.
  [[self window] drawCustomFrameRect:rect forView:self];
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
