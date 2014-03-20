// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/notifications/balloon_view_host_mac.h"

#import <Cocoa/Cocoa.h>

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

BalloonViewHost::BalloonViewHost(Balloon* balloon)
    : BalloonHost(balloon) {
}

BalloonViewHost::~BalloonViewHost() {
   Shutdown();
}

void BalloonViewHost::UpdateActualSize(const gfx::Size& new_size) {
  // Update the size of the web contents view.
  // The view's new top left corner should be identical to the view's old top
  // left corner.
  NSView* web_contents_view = web_contents_->GetView()->GetNativeView();
  NSRect old_wcv_frame = [web_contents_view frame];
  CGFloat new_x = old_wcv_frame.origin.x;
  CGFloat new_y =
      old_wcv_frame.origin.y + (old_wcv_frame.size.height - new_size.height());
  NSRect new_wcv_frame =
      NSMakeRect(new_x, new_y, new_size.width(), new_size.height());
  [web_contents_view setFrame:new_wcv_frame];

  // Update the size of the balloon view.
  NSView* view = native_view();
  NSRect frame = [view frame];
  frame.size.width = new_size.width();
  frame.size.height = new_size.height();

  [view setFrame:frame];
  [view setNeedsDisplay:YES];
}

gfx::NativeView BalloonViewHost::native_view() const {
  return web_contents_->GetView()->GetContentNativeView();
}
