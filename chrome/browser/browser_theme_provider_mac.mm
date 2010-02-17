// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_theme_provider.h"

#import <Cocoa/Cocoa.h>

#include "app/gfx/color_utils.h"
#include "base/logging.h"
#include "chrome/browser/browser_theme_pack.h"
#include "skia/ext/skia_utils_mac.h"

NSString* const kBrowserThemeDidChangeNotification =
    @"BrowserThemeDidChangeNotification";

namespace {

void HSLToHSB(const color_utils::HSL& hsl, CGFloat* h, CGFloat* s, CGFloat* b) {
  SkColor color = color_utils::HSLToSkColor(hsl, 255);  // alpha doesn't matter
  SkScalar hsv[3];
  SkColorToHSV(color, hsv);

  *h = SkScalarToDouble(hsv[0]) / 360.0;
  *s = SkScalarToDouble(hsv[1]);
  *b = SkScalarToDouble(hsv[2]);
}

}

NSImage* BrowserThemeProvider::GetNSImageNamed(int id,
                                               bool allow_default) const {
  DCHECK(CalledOnValidThread());

  if (!allow_default && !HasCustomImage(id))
    return nil;

  // Check to see if we already have the image in the cache.
  NSImageMap::const_iterator nsimage_iter = nsimage_cache_.find(id);
  if (nsimage_iter != nsimage_cache_.end())
    return nsimage_iter->second;

  // Why don't we load the file directly into the image instead of the whole
  // SkBitmap > native conversion?
  // - For consistency with other platforms.
  // - To get the generated tinted images.
  SkBitmap* bitmap = GetBitmapNamed(id);
  NSImage* nsimage = gfx::SkBitmapToNSImage(*bitmap);

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

NSColor* BrowserThemeProvider::GetNSColor(int id,
                                          bool allow_default) const {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the color in the cache.
  NSColorMap::const_iterator nscolor_iter = nscolor_cache_.find(id);
  if (nscolor_iter != nscolor_cache_.end()) {
    bool cached_is_default = nscolor_iter->second.second;
    if (!cached_is_default || allow_default)
      return nscolor_iter->second.first;
  }

  bool is_default = false;
  SkColor sk_color;
  if (theme_pack_.get() && theme_pack_->GetColor(id, &sk_color)) {
    is_default = false;
  } else {
    is_default = true;
    sk_color = GetDefaultColor(id);
  }

  if (is_default && !allow_default)
    return nil;

  NSColor* color = [NSColor
      colorWithCalibratedRed:SkColorGetR(sk_color)/255.0
                       green:SkColorGetG(sk_color)/255.0
                        blue:SkColorGetB(sk_color)/255.0
                       alpha:SkColorGetA(sk_color)/255.0];

  // We loaded successfully.  Cache the color.
  if (color) {
    nscolor_cache_[id] = std::make_pair([color retain], is_default);
    return color;
  }

  return nil;
}

NSColor* BrowserThemeProvider::GetNSColorTint(int id,
                                              bool allow_default) const {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the color in the cache.
  NSColorMap::const_iterator nscolor_iter = nscolor_cache_.find(id);
  if (nscolor_iter != nscolor_cache_.end()) {
    bool cached_is_default = nscolor_iter->second.second;
    if (!cached_is_default || allow_default)
      return nscolor_iter->second.first;
  }

  bool is_default = false;
  color_utils::HSL tint;
  if (theme_pack_.get() && theme_pack_->GetTint(id, &tint)) {
    is_default = false;
  } else {
    is_default = true;
    tint = GetDefaultTint(id);
  }

  if (is_default && !allow_default)
    return nil;

  CGFloat hue, saturation, brightness;
  HSLToHSB(tint, &hue, &saturation, &brightness);

  NSColor* tint_color = [NSColor colorWithCalibratedHue:hue
                                             saturation:saturation
                                             brightness:brightness
                                                  alpha:1.0];

  // We loaded successfully.  Cache the color.
  if (tint_color) {
    nscolor_cache_[id] = std::make_pair([tint_color retain], is_default);
    return tint_color;
  }

  return nil;
}

// Let all the browser views know that themes have changed in a platform way.
void BrowserThemeProvider::NotifyPlatformThemeChanged() {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter postNotificationName:kBrowserThemeDidChangeNotification
                               object:[NSValue valueWithPointer:this]];
}

void BrowserThemeProvider::FreePlatformCaches() {
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
    [i->second.first release];
  }
  nscolor_cache_.clear();
}
