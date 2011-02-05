// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_snapshot/window_snapshot.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/scoped_nsobject.h"
#include "ui/gfx/rect.h"

namespace browser {

gfx::Rect GrabWindowSnapshot(gfx::NativeWindow window,
    std::vector<unsigned char>* png_representation) {
  png_representation->clear();

  // Make sure to grab the "window frame" view so we get current tab +
  // tabstrip.
  NSView* view = [[window contentView] superview];
  base::mac::ScopedCFTypeRef<CGImageRef> windowSnapshot(CGWindowListCreateImage(
      CGRectNull, kCGWindowListOptionIncludingWindow,
      [[view window] windowNumber], kCGWindowImageBoundsIgnoreFraming));
  if (CGImageGetWidth(windowSnapshot) <= 0)
    return gfx::Rect();

  scoped_nsobject<NSBitmapImageRep> rep(
      [[NSBitmapImageRep alloc] initWithCGImage:windowSnapshot]);
  NSData* data = [rep representationUsingType:NSPNGFileType properties:nil];
  const unsigned char* buf = static_cast<const unsigned char*>([data bytes]);
  NSUInteger length = [data length];
  if (buf == NULL || length == 0)
    return gfx::Rect();

  png_representation->assign(buf, buf + length);
  DCHECK(png_representation->size() > 0);

  return gfx::Rect(static_cast<int>([rep pixelsWide]),
                   static_cast<int>([rep pixelsHigh]));
}

}  // namespace browser
