// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PANELS_PANEL_UTILS_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_PANELS_PANEL_UTILS_COCOA_H_

#import <Cocoa/Cocoa.h>
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace cocoa_utils {

// TODO(dcheng): Move elsewhere so BrowserWindowCocoa can use them too.
// Converts a rect from the platform-independent screen coordinates (with the
// (0,0) in the top-left corner of the primary screen) to the Cocoa screen
// coordinates (with (0,0) in the low-left corner).
NSRect ConvertRectToCocoaCoordinates(const gfx::Rect& bounds);

// Converts a rect from the Cocoa screen oordinates (with (0,0) in the low-left
// corner) to the platform-independent screen coordinates (with the (0,0) in
// the top-left corner of the primary screen).
gfx::Rect ConvertRectFromCocoaCoordinates(NSRect bounds);

// Converts a point from the platform-independent screen coordinates (with the
// (0,0) in the top-left corner of the primary screen) to the Cocoa screen
// coordinates (with (0,0) in the low-left corner).
NSPoint ConvertPointToCocoaCoordinates(const gfx::Point& point);

// Converts a point from the Cocoa screen coordinates (with (0,0) in the
// low-left corner of the primary screen) to the platform-independent screen
// coordinates (with the (0,0) in the top-left corner).
gfx::Point ConvertPointFromCocoaCoordinates(NSPoint point);

}  // namespace cocoa_utils

#endif  // CHROME_BROWSER_UI_COCOA_PANELS_PANEL_UTILS_COCOA_H_
