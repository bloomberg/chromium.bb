// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/mac/desktop_configuration.h"

#include <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "skia/ext/skia_utils_mac.h"

#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

@interface NSScreen (LionAPI)
- (CGFloat)backingScaleFactor;
- (NSRect)convertRectToBacking:(NSRect)aRect;
@end

#endif  // 10.7

namespace media {

namespace {

SkIRect NSRectToSkIRect(const NSRect& ns_rect) {
  SkIRect result;
  gfx::CGRectToSkRect(NSRectToCGRect(ns_rect)).roundOut(&result);
  return result;
}

// Inverts the position of |rect| from bottom-up coordinates to top-down,
// relative to |bounds|.
void InvertRectYOrigin(const SkIRect& bounds, SkIRect* rect) {
  DCHECK_EQ(0, bounds.top());
  rect->setXYWH(rect->x(), bounds.bottom() - rect->bottom(),
                rect->width(), rect->height());
}

MacDisplayConfiguration GetConfigurationForScreen(NSScreen* screen) {
  MacDisplayConfiguration display_config;

  // Fetch the NSScreenNumber, which is also the CGDirectDisplayID.
  NSDictionary* device_description = [screen deviceDescription];
  display_config.id = static_cast<CGDirectDisplayID>(
      [[device_description objectForKey:@"NSScreenNumber"] intValue]);

  // Determine the display's logical & physical dimensions.
  NSRect ns_bounds = [screen frame];
  display_config.bounds = NSRectToSkIRect(ns_bounds);

  // If the host is running Mac OS X 10.7+ or later, query the scaling factor
  // between logical and physical (aka "backing") pixels, otherwise assume 1:1.
  if ([screen respondsToSelector:@selector(backingScaleFactor)] &&
      [screen respondsToSelector:@selector(convertRectToBacking:)]) {
    display_config.dip_to_pixel_scale = [screen backingScaleFactor];
    NSRect ns_pixel_bounds = [screen convertRectToBacking: ns_bounds];
    display_config.pixel_bounds = NSRectToSkIRect(ns_pixel_bounds);
  } else {
    display_config.pixel_bounds = display_config.bounds;
  }

  return display_config;
}

} // namespace

MacDisplayConfiguration::MacDisplayConfiguration()
    : id(0),
      bounds(SkIRect::MakeEmpty()),
      pixel_bounds(SkIRect::MakeEmpty()),
      dip_to_pixel_scale(1.0f) {
}

MacDesktopConfiguration::MacDesktopConfiguration()
    : bounds(SkIRect::MakeEmpty()),
      pixel_bounds(SkIRect::MakeEmpty()),
      dip_to_pixel_scale(1.0f) {
}

MacDesktopConfiguration::~MacDesktopConfiguration() {
}

// static
MacDesktopConfiguration MacDesktopConfiguration::GetCurrent(Origin origin) {
  MacDesktopConfiguration desktop_config;

  NSArray* screens = [NSScreen screens];
  CHECK(screens != NULL);

  // Iterator over the monitors, adding the primary monitor and monitors whose
  // DPI match that of the primary monitor.
  for (NSUInteger i = 0; i < [screens count]; ++i) {
    MacDisplayConfiguration display_config
        = GetConfigurationForScreen([screens objectAtIndex: i]);

    // Handling mixed-DPI is hard, so we only return displays that match the
    // "primary" display's DPI.  The primary display is always the first in the
    // list returned by [NSScreen screens].
    if (i == 0) {
      desktop_config.dip_to_pixel_scale = display_config.dip_to_pixel_scale;
    } else if (desktop_config.dip_to_pixel_scale !=
               display_config.dip_to_pixel_scale) {
      continue;
    }

    // Cocoa uses bottom-up coordinates, so if the caller wants top-down then
    // we need to invert the positions of secondary monitors relative to the
    // primary one (the primary monitor's position is (0,0) in both systems).
    if (i > 0 && origin == TopLeftOrigin) {
      InvertRectYOrigin(desktop_config.displays[0].bounds,
                        &display_config.bounds);
      InvertRectYOrigin(desktop_config.displays[0].pixel_bounds,
                        &display_config.pixel_bounds);
    }

    // Add the display to the configuration.
    desktop_config.displays.push_back(display_config);

    // Update the desktop bounds to account for this display.
    desktop_config.bounds.join(display_config.bounds);
    desktop_config.pixel_bounds.join(display_config.pixel_bounds);
  }

  return desktop_config;
}

}  // namespace media
