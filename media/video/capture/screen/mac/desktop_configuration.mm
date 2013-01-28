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

MacDisplayConfiguration::MacDisplayConfiguration()
  : id(0),
    bounds(SkIRect::MakeEmpty()),
    pixel_bounds(SkIRect::MakeEmpty()),
    dip_to_pixel_scale(1.0f) {
}

static SkIRect NSRectToSkIRect(const NSRect& ns_rect) {
  SkIRect result;
  gfx::CGRectToSkRect(NSRectToCGRect(ns_rect)).roundOut(&result);
  return result;
}

static MacDisplayConfiguration GetConfigurationForScreen(NSScreen* screen) {
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

MacDesktopConfiguration::MacDesktopConfiguration()
  : bounds(SkIRect::MakeEmpty()),
    pixel_bounds(SkIRect::MakeEmpty()),
    dip_to_pixel_scale(1.0f) {
}

MacDesktopConfiguration::~MacDesktopConfiguration() {
}

// static
MacDesktopConfiguration MacDesktopConfiguration::GetCurrent() {
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

    // Add the display to the configuration.
    desktop_config.displays.push_back(display_config);

    // Update the desktop bounds to account for this display.
    desktop_config.bounds.join(display_config.bounds);
    desktop_config.pixel_bounds.join(display_config.pixel_bounds);
  }

  return desktop_config;
}

}  // namespace media
