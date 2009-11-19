// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/extension_view_mac.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"

ExtensionViewMac::ExtensionViewMac(ExtensionHost* extension_host,
                                   Browser* browser)
    : is_toolstrip_(true),
      browser_(browser),
      extension_host_(extension_host),
      render_widget_host_view_(NULL) {
  DCHECK(extension_host_);
}

ExtensionViewMac::~ExtensionViewMac() {
  if (render_widget_host_view_)
    [render_widget_host_view_->native_view() release];
}

void ExtensionViewMac::Init() {
  CreateWidgetHostView();
}

gfx::NativeView ExtensionViewMac::native_view() {
  DCHECK(render_widget_host_view_);
  return render_widget_host_view_->native_view();
}

RenderViewHost* ExtensionViewMac::render_view_host() const {
  return extension_host_->render_view_host();
}

void ExtensionViewMac::SetBackground(const SkBitmap& background) {
  DCHECK(render_widget_host_view_);
  if (render_view_host()->IsRenderViewLive()) {
    render_widget_host_view_->SetBackground(background);
  } else {
    pending_background_ = background;
  }
}

void ExtensionViewMac::UpdatePreferredSize(const gfx::Size& new_size) {
  // TODO(thakis, erikkay): Windows does some tricks to resize the extension
  // view not before it's visible. Do something similar here.

  // No need to use CA here, our caller calls us repeatedly to animate the
  // resizing.
  NSView* view = native_view();
  NSRect frame = [view frame];
  frame.size.width = new_size.width();

  // RenderWidgetHostViewCocoa overrides setFrame but not setFrameSize.
  [view setFrame:frame];
  [view setNeedsDisplay:YES];
}

void ExtensionViewMac::RenderViewCreated() {
  if (!pending_background_.empty() && render_view_host()->view()) {
    render_widget_host_view_->SetBackground(pending_background_);
    pending_background_.reset();
  }
}

void ExtensionViewMac::CreateWidgetHostView() {
  DCHECK(!render_widget_host_view_);
  render_widget_host_view_ = new RenderWidgetHostViewMac(render_view_host());

  // The RenderWidgetHostViewMac is owned by its native view, which is created
  // in an autoreleased state. retain it, so that it doesn't immediately
  // disappear.
  [render_widget_host_view_->native_view() retain];

  extension_host_->CreateRenderView(render_widget_host_view_);
}
