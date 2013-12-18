// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "chrome/browser/themes/theme_properties.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMNSColor+Luminance.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "skia/ext/skia_utils_mac.h"

NSString* const kBrowserThemeDidChangeNotification =
    @"BrowserThemeDidChangeNotification";

typedef ThemeProperties Properties;

namespace {

void HSLToHSB(const color_utils::HSL& hsl, CGFloat* h, CGFloat* s, CGFloat* b) {
  SkColor color = color_utils::HSLToSkColor(hsl, 255);  // alpha doesn't matter
  SkScalar hsv[3];
  SkColorToHSV(color, hsv);

  *h = SkScalarToDouble(hsv[0]) / 360.0;
  *s = SkScalarToDouble(hsv[1]);
  *b = SkScalarToDouble(hsv[2]);
}

}  // namespace

NSImage* ThemeService::GetNSImageNamed(int id) const {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the image in the cache.
  NSImageMap::const_iterator nsimage_iter = nsimage_cache_.find(id);
  if (nsimage_iter != nsimage_cache_.end())
    return nsimage_iter->second;

  // Why don't we load the file directly into the image instead of the whole
  // gfx::Image > native conversion?
  // - For consistency with other platforms.
  // - To get the generated tinted images.
  NSImage* nsimage = nil;
  if (theme_supplier_.get()) {
    gfx::Image image = theme_supplier_->GetImageNamed(id);
    if (!image.IsEmpty())
      nsimage = image.ToNSImage();
  }

  // If the theme didn't override this image then load it from the resource
  // bundle.
  if (!nsimage) {
    nsimage = rb_.GetNativeImageNamed(id).ToNSImage();
  }

  // We loaded successfully.  Cache the image.
  if (nsimage) {
    nsimage_cache_[id] = [nsimage retain];
    return nsimage;
  }

  // We failed to retrieve the bitmap, show a debugging red square.
  LOG(WARNING) << "Unable to load NSImage with id " << id;
  NOTREACHED();  // Want to assert in debug mode.

  static NSImage* empty_image = NULL;
  if (!empty_image) {
    // The placeholder image is bright red so people notice the problem.  This
    // image will be leaked, but this code should never be hit.
    NSRect image_rect = NSMakeRect(0, 0, 32, 32);
    empty_image = [[NSImage alloc] initWithSize:image_rect.size];
    [empty_image lockFocus];
    [[NSColor redColor] set];
    NSRectFill(image_rect);
    [empty_image unlockFocus];
  }

  return empty_image;
}

NSColor* ThemeService::GetNSImageColorNamed(int id) const {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the color in the cache.
  NSColorMap::const_iterator nscolor_iter = nscolor_cache_.find(id);
  if (nscolor_iter != nscolor_cache_.end())
    return nscolor_iter->second;

  NSImage* image = GetNSImageNamed(id);
  if (!image)
    return nil;
  NSColor* image_color = [NSColor colorWithPatternImage:image];

  // We loaded successfully.  Cache the color.
  if (image_color)
    nscolor_cache_[id] = [image_color retain];

  return image_color;
}

NSColor* ThemeService::GetNSColor(int id) const {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the color in the cache.
  NSColorMap::const_iterator nscolor_iter = nscolor_cache_.find(id);
  if (nscolor_iter != nscolor_cache_.end())
    return nscolor_iter->second;

  SkColor sk_color = GetColor(id);
  NSColor* color = gfx::SkColorToCalibratedNSColor(sk_color);

  // We loaded successfully.  Cache the color.
  if (color)
    nscolor_cache_[id] = [color retain];

  return color;
}

NSColor* ThemeService::GetNSColorTint(int id) const {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the color in the cache.
  NSColorMap::const_iterator nscolor_iter = nscolor_cache_.find(id);
  if (nscolor_iter != nscolor_cache_.end())
    return nscolor_iter->second;

  color_utils::HSL tint = GetTint(id);
  NSColor* tint_color = nil;
  if (tint.h == -1 && tint.s == -1 && tint.l == -1) {
    tint_color = [NSColor blackColor];
  } else {
    CGFloat hue, saturation, brightness;
    HSLToHSB(tint, &hue, &saturation, &brightness);

    tint_color = [NSColor colorWithCalibratedHue:hue
                                      saturation:saturation
                                      brightness:brightness
                                           alpha:1.0];
  }

  // We loaded successfully.  Cache the color.
  if (tint_color)
    nscolor_cache_[id] = [tint_color retain];

  return tint_color;
}

NSGradient* ThemeService::GetNSGradient(int id) const {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the gradient in the cache.
  NSGradientMap::const_iterator nsgradient_iter = nsgradient_cache_.find(id);
  if (nsgradient_iter != nsgradient_cache_.end())
    return nsgradient_iter->second;

  NSGradient* gradient = nil;

  // Note that we are not leaking when we assign a retained object to
  // |gradient|; in all cases we cache it before we return.
  switch (id) {
    case Properties::GRADIENT_FRAME_INCOGNITO:
    case Properties::GRADIENT_FRAME_INCOGNITO_INACTIVE: {
      // TODO(avi): can we simplify this?
      BOOL active = id == Properties::GRADIENT_FRAME_INCOGNITO;
      NSColor* base_color = [NSColor colorWithCalibratedRed:83/255.0
                                                      green:108.0/255.0
                                                       blue:140/255.0
                                                      alpha:1.0];

      NSColor *start_color =
          [base_color gtm_colorAdjustedFor:GTMColorationBaseMidtone
                                     faded:!active];
      NSColor *end_color =
          [base_color gtm_colorAdjustedFor:GTMColorationBaseShadow
                                     faded:!active];

      if (!active) {
        start_color = [start_color gtm_colorByAdjustingLuminance:0.1
                                                      saturation:0.5];
        end_color = [end_color gtm_colorByAdjustingLuminance:0.1
                                                  saturation:0.5];
      }

      gradient = [[NSGradient alloc] initWithStartingColor:start_color
                                               endingColor:end_color];
      break;
    }

    case Properties::GRADIENT_TOOLBAR:
    case Properties::GRADIENT_TOOLBAR_INACTIVE: {
      NSColor* base_color = [NSColor colorWithCalibratedWhite:0.2 alpha:1.0];
      BOOL faded = (id == Properties::GRADIENT_TOOLBAR_INACTIVE ) ||
                   (id == Properties::GRADIENT_TOOLBAR_BUTTON_INACTIVE);
      NSColor* start_color =
          [base_color gtm_colorAdjustedFor:GTMColorationLightHighlight
                                     faded:faded];
      NSColor* mid_color =
          [base_color gtm_colorAdjustedFor:GTMColorationLightMidtone
                                     faded:faded];
      NSColor* end_color =
          [base_color gtm_colorAdjustedFor:GTMColorationLightShadow
                                     faded:faded];
      NSColor* glow_color =
          [base_color gtm_colorAdjustedFor:GTMColorationLightPenumbra
                                     faded:faded];

      gradient =
          [[NSGradient alloc] initWithColorsAndLocations:start_color, 0.0,
                                                         mid_color, 0.25,
                                                         end_color, 0.5,
                                                         glow_color, 0.75,
                                                         nil];
      break;
    }

    case Properties::GRADIENT_TOOLBAR_BUTTON:
    case Properties::GRADIENT_TOOLBAR_BUTTON_INACTIVE: {
      NSColor* start_color = [NSColor colorWithCalibratedWhite:1.0 alpha:0.0];
      NSColor* end_color = [NSColor colorWithCalibratedWhite:1.0 alpha:0.3];
      gradient = [[NSGradient alloc] initWithStartingColor:start_color
                                               endingColor:end_color];
      break;
    }
    case Properties::GRADIENT_TOOLBAR_BUTTON_PRESSED:
    case Properties::GRADIENT_TOOLBAR_BUTTON_PRESSED_INACTIVE: {
      NSColor* base_color = [NSColor colorWithCalibratedWhite:0.5 alpha:1.0];
      BOOL faded = id == Properties::GRADIENT_TOOLBAR_BUTTON_PRESSED_INACTIVE;
      NSColor* start_color =
          [base_color gtm_colorAdjustedFor:GTMColorationBaseShadow
                                     faded:faded];
      NSColor* end_color =
          [base_color gtm_colorAdjustedFor:GTMColorationBaseMidtone
                                     faded:faded];

      gradient = [[NSGradient alloc] initWithStartingColor:start_color
                                               endingColor:end_color];
      break;
    }
    default:
      LOG(WARNING) << "Gradient request with unknown id " << id;
      NOTREACHED();  // Want to assert in debug mode.
      break;
  }

  // We loaded successfully.  Cache the gradient.
  if (gradient)
    nsgradient_cache_[id] = gradient;  // created retained

  return gradient;
}

// Let all the browser views know that themes have changed in a platform way.
void ThemeService::NotifyPlatformThemeChanged() {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter postNotificationName:kBrowserThemeDidChangeNotification
                               object:[NSValue valueWithPointer:this]];
}

void ThemeService::FreePlatformCaches() {
  DCHECK(CalledOnValidThread());

  // Free images.
  for (NSImageMap::iterator i = nsimage_cache_.begin();
       i != nsimage_cache_.end(); i++) {
    [i->second release];
  }
  nsimage_cache_.clear();

  // Free colors.
  for (NSColorMap::iterator i = nscolor_cache_.begin();
       i != nscolor_cache_.end(); i++) {
    [i->second release];
  }
  nscolor_cache_.clear();

  // Free gradients.
  for (NSGradientMap::iterator i = nsgradient_cache_.begin();
       i != nsgradient_cache_.end(); i++) {
    [i->second release];
  }
  nsgradient_cache_.clear();
}
