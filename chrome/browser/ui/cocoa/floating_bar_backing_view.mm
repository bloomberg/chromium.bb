// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/floating_bar_backing_view.h"

#import "ui/base/cocoa/appkit_utils.h"

@implementation FloatingBarBackingView

// Eat all mouse events (and do *not* pass them on to the next responder!).
- (void)mouseDown:(NSEvent*)event {}
- (void)rightMouseDown:(NSEvent*)event {}
- (void)otherMouseDown:(NSEvent*)event {}
- (void)rightMouseUp:(NSEvent*)event {}
- (void)otherMouseUp:(NSEvent*)event {}
- (void)mouseMoved:(NSEvent*)event {}
- (void)mouseDragged:(NSEvent*)event {}
- (void)rightMouseDragged:(NSEvent*)event {}
- (void)otherMouseDragged:(NSEvent*)event {}

// Eat this too, except that ...
- (void)mouseUp:(NSEvent*)event {
  // a double-click in the blank area should try to minimize, to be consistent
  // with double-clicks on the contiguous tab strip area. (It'll fail and beep.)
  if ([event clickCount] == 2)
    ui::WindowTitlebarReceivedDoubleClick([self window], self);
}

@end  // @implementation FloatingBarBackingView
