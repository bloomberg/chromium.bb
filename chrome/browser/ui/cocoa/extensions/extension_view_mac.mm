// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"

// The minimum/maximum dimensions of the popup.
const CGFloat ExtensionViewMac::kMinWidth = 25.0;
const CGFloat ExtensionViewMac::kMinHeight = 25.0;
const CGFloat ExtensionViewMac::kMaxWidth = 800.0;
const CGFloat ExtensionViewMac::kMaxHeight = 600.0;

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
  frame.size.height = new_size.height();

  // |new_size| is in pixels. Convert to view units.
  frame.size = [view convertSize:frame.size fromView:nil];

  // On first display of some extensions, this function is called with zero
  // width after the correct size has been set. Bail if zero is seen, assuming
  // that an extension's view doesn't want any dimensions to ever be zero.
  // TODO(andybons): Verify this assumption and look into WebCore's
  // |contentesPreferredWidth| to see why this is occurring.
  if (NSIsEmptyRect(frame))
    return;

  DCHECK([view isKindOfClass:[RenderWidgetHostViewCocoa class]]);
  RenderWidgetHostViewCocoa* hostView = (RenderWidgetHostViewCocoa*)view;

  // RenderWidgetHostViewCocoa overrides setFrame but not setFrameSize.
  // We need to defer the update back to the RenderWidgetHost so we don't
  // get the flickering effect on 10.5 of http://crbug.com/31970
  [hostView setFrameWithDeferredUpdate:frame];
  [hostView setNeedsDisplay:YES];
}

void ExtensionViewMac::RenderViewCreated() {
  // Do not allow webkit to draw scroll bars on views smaller than
  // the largest size view allowed.  The view will be resized to make
  // scroll bars unnecessary.  Scroll bars change the height of the
  // view, so not drawing them is necessary to avoid infinite resizing.
  gfx::Size largest_popup_size(
      CGSizeMake(ExtensionViewMac::kMaxWidth, ExtensionViewMac::kMaxHeight));
  extension_host_->DisableScrollbarsForSmallWindows(largest_popup_size);

  if (!pending_background_.empty() && render_view_host()->view()) {
    render_widget_host_view_->SetBackground(pending_background_);
    pending_background_.reset();
  }
}

void ExtensionViewMac::WindowFrameChanged() {
  if (render_widget_host_view_)
    render_widget_host_view_->WindowFrameChanged();
}

void ExtensionViewMac::CreateWidgetHostView() {
  DCHECK(!render_widget_host_view_);
  render_widget_host_view_ = new RenderWidgetHostViewMac(render_view_host());

  // The RenderWidgetHostViewMac is owned by its native view, which is created
  // in an autoreleased state. retain it, so that it doesn't immediately
  // disappear.
  [render_widget_host_view_->native_view() retain];

  extension_host_->CreateRenderViewSoon(render_widget_host_view_);
}
