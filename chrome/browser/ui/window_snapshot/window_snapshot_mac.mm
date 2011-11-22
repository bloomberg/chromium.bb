// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_snapshot/window_snapshot.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_nsobject.h"
#include "ui/gfx/rect.h"

namespace browser {

bool GrabWindowSnapshot(gfx::NativeWindow window,
    std::vector<unsigned char>* png_representation,
    const gfx::Rect& snapshot_bounds) {
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  gfx::Rect screen_bounds = gfx::Rect(NSRectToCGRect([screen frame]));
  gfx::Rect window_bounds = gfx::Rect(NSRectToCGRect([window frame]));

  // Flip window coordinates based on the primary screen.
  window_bounds.set_y(
      screen_bounds.height() - window_bounds.y() - window_bounds.height());

  // Convert snapshot bounds relative to window into bounds relative to
  // screen.
  gfx::Rect screen_snapshot_bounds = gfx::Rect(
      window_bounds.origin().Add(snapshot_bounds.origin()),
      snapshot_bounds.size());

  DCHECK_LE(screen_snapshot_bounds.right(), window_bounds.right());
  DCHECK_LE(screen_snapshot_bounds.bottom(), window_bounds.bottom());

  png_representation->clear();

  // Make sure to grab the "window frame" view so we get current tab +
  // tabstrip.
  NSView* view = [[window contentView] superview];
  base::mac::ScopedCFTypeRef<CGImageRef> windowSnapshot(CGWindowListCreateImage(
      screen_snapshot_bounds.ToCGRect(), kCGWindowListOptionIncludingWindow,
      [[view window] windowNumber], kCGWindowImageBoundsIgnoreFraming));
  if (CGImageGetWidth(windowSnapshot) <= 0)
    return false;

  scoped_nsobject<NSBitmapImageRep> rep(
      [[NSBitmapImageRep alloc] initWithCGImage:windowSnapshot]);
  NSData* data = [rep representationUsingType:NSPNGFileType properties:nil];
  const unsigned char* buf = static_cast<const unsigned char*>([data bytes]);
  NSUInteger length = [data length];
  if (buf == NULL || length == 0)
    return false;

  png_representation->assign(buf, buf + length);
  DCHECK(!png_representation->empty());

  return true;
}

}  // namespace browser
