// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/frame/native_widget_mac_frameless_nswindow.h"

@implementation NativeWidgetMacFramelessNSWindow

+ (NSRect)frameRectForContentRect:(NSRect)contentRect
                        styleMask:(NSUInteger)mask {
  return contentRect;
}

+ (NSRect)contentRectForFrameRect:(NSRect)frameRect styleMask:(NSUInteger)mask {
  return frameRect;
}

- (NSRect)frameRectForContentRect:(NSRect)contentRect {
  return contentRect;
}

- (NSRect)contentRectForFrameRect:(NSRect)frameRect {
  return frameRect;
}

@end
