// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/notifications/balloon_view_host_mac.h"

#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"

BalloonViewHost::BalloonViewHost(Balloon* balloon)
    : BalloonHost(balloon) {
}

BalloonViewHost::~BalloonViewHost() {
   Shutdown();
}

void BalloonViewHost::UpdateActualSize(const gfx::Size& new_size) {
  tab_contents_->view()->SizeContents(new_size);
  NSView* view = native_view();
  NSRect frame = [view frame];
  frame.size.width = new_size.width();
  frame.size.height = new_size.height();

  [view setFrame:frame];
  [view setNeedsDisplay:YES];
}

gfx::NativeView BalloonViewHost::native_view() const {
  return tab_contents_->GetContentNativeView();
}
