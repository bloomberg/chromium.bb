// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/panels/panel_utils_cocoa.h"

namespace cocoa_utils {

NSRect ConvertRectToCocoaCoordinates(const gfx::Rect& bounds) {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];

  return NSMakeRect(
      bounds.x(), NSHeight([screen frame]) - bounds.height() - bounds.y(),
      bounds.width(), bounds.height());
}

gfx::Rect ConvertRectFromCocoaCoordinates(NSRect bounds) {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];

  return gfx::Rect(
      NSMinX(bounds), NSHeight([screen frame]) - NSMaxY(bounds),
      NSWidth(bounds), NSHeight(bounds));
}

NSPoint ConvertPointToCocoaCoordinates(const gfx::Point& point) {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];

  return NSMakePoint(point.x(), NSHeight([screen frame]) - point.y());
}

gfx::Point ConvertPointFromCocoaCoordinates(NSPoint point) {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];

  return gfx::Point(point.x, NSHeight([screen frame]) - point.y);
}

}  // namespace cocoa_utils
