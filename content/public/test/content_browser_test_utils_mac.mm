// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test_utils.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "content/browser/renderer_host/text_input_client_mac.h"
#include "content/public/browser/render_widget_host.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace content {

void SetWindowBounds(gfx::NativeWindow window, const gfx::Rect& bounds) {
  NSRect new_bounds = NSRectFromCGRect(bounds.ToCGRect());
  if ([[NSScreen screens] count] > 0) {
    new_bounds.origin.y =
        [[[NSScreen screens] firstObject] frame].size.height -
        new_bounds.origin.y - new_bounds.size.height;
  }

  [window setFrame:new_bounds display:NO];
}

void GetStringAtPointForRenderWidget(
    RenderWidgetHost* rwh,
    const gfx::Point& point,
    base::Callback<void(const std::string&, const gfx::Point&)>
        result_callback) {
  TextInputClientMac::GetInstance()->GetStringAtPoint(
      rwh, point, ^(NSAttributedString* string, NSPoint baselinePoint) {
        result_callback.Run([[string string] UTF8String],
                            gfx::Point(baselinePoint.x, baselinePoint.y));
      });
}

void GetStringFromRangeForRenderWidget(
    RenderWidgetHost* rwh,
    const gfx::Range& range,
    base::Callback<void(const std::string&, const gfx::Point&)>
        result_callback) {
  TextInputClientMac::GetInstance()->GetStringFromRange(
      rwh, range.ToNSRange(),
      ^(NSAttributedString* string, NSPoint baselinePoint) {
        result_callback.Run([[string string] UTF8String],
                            gfx::Point(baselinePoint.x, baselinePoint.y));
      });
}

}  // namespace content
