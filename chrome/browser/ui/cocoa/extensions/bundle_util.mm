// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/bundle_util.h"

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace {

const CGFloat kExtensionIconSize = 32;

}  // namespace

CGFloat PopulateBundleItemsList(
    const extensions::BundleInstaller::ItemList& items,
    NSView* items_field) {
  const CGFloat title_width =
      [items_field frame].size.width - kExtensionIconSize;
  CGFloat offset = 0;
  // Go over the items backwards, since Cocoa coords go from the bottom up.
  for (size_t i = items.size(); i > 0; --i) {
    const extensions::BundleInstaller::Item& item = items[i - 1];

    NSString* title = base::SysUTF16ToNSString(item.GetNameForDisplay());
    base::scoped_nsobject<NSTextField> title_view([[NSTextField alloc]
        initWithFrame:NSMakeRect(kExtensionIconSize, offset, title_width, 0)]);
    [title_view setBordered:NO];
    [title_view setEditable:NO];
    [title_view setStringValue:title];
    [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:title_view];

    NSRect title_frame = [title_view frame];
    NSRect icon_frame =
        NSMakeRect(0, offset, kExtensionIconSize, kExtensionIconSize);

    // Vertically center-align icon and title.
    CGFloat align = (icon_frame.size.height - title_frame.size.height) / 2;
    if (align > 0) {
      title_frame.origin.y += align;
      [title_view setFrame:title_frame];
    } else {
      icon_frame.origin.y -= align;
    }

    gfx::ImageSkia skia_image = gfx::ImageSkia::CreateFrom1xBitmap(item.icon);
    NSImage* image = gfx::NSImageFromImageSkiaWithColorSpace(
        skia_image, base::mac::GetSystemColorSpace());
    base::scoped_nsobject<NSImageView> icon_view(
        [[NSImageView alloc] initWithFrame:icon_frame]);
    [icon_view setImage:image];

    [items_field addSubview:icon_view];
    [items_field addSubview:title_view];

    offset = NSMaxY(NSUnionRect(title_frame, icon_frame));
  }
  return offset;
}
