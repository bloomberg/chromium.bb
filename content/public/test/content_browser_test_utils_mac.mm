// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test_utils.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "ui/gfx/rect.h"

namespace content {

void SetWindowBounds(gfx::NativeWindow window, const gfx::Rect& bounds) {
  NSRect new_bounds = NSRectFromCGRect(bounds.ToCGRect());
  if ([[NSScreen screens] count] > 0) {
    new_bounds.origin.y =
        [[[NSScreen screens] objectAtIndex:0] frame].size.height -
        new_bounds.origin.y - new_bounds.size.height;
  }

  [window setFrame:new_bounds display:NO];
}

}  // namespace content
