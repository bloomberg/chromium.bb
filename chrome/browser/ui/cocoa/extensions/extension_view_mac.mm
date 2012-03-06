// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"

#include "chrome/browser/extensions/extension_host.h"
#include "content/browser/tab_contents/web_contents_view_mac.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

// The minimum/maximum dimensions of the popup.
const CGFloat ExtensionViewMac::kMinWidth = 25.0;
const CGFloat ExtensionViewMac::kMinHeight = 25.0;
const CGFloat ExtensionViewMac::kMaxWidth = 800.0;
const CGFloat ExtensionViewMac::kMaxHeight = 600.0;

ExtensionViewMac::ExtensionViewMac(ExtensionHost* extension_host,
                                   Browser* browser)
    : browser_(browser),
      extension_host_(extension_host),
      container_(NULL) {
  DCHECK(extension_host_);
  [native_view() setHidden:YES];
}

ExtensionViewMac::~ExtensionViewMac() {
}

void ExtensionViewMac::Init() {
  CreateWidgetHostView();
}

gfx::NativeView ExtensionViewMac::native_view() {
  return extension_host_->host_contents()->GetView()->GetNativeView();
}

RenderViewHost* ExtensionViewMac::render_view_host() const {
  return extension_host_->render_view_host();
}

void ExtensionViewMac::DidStopLoading() {
  ShowIfCompletelyLoaded();
}

void ExtensionViewMac::SetBackground(const SkBitmap& background) {
  if (!pending_background_.empty() && render_view_host()->GetView()) {
    render_view_host()->GetView()->SetBackground(background);
  } else {
    pending_background_ = background;
  }
  ShowIfCompletelyLoaded();
}

void ExtensionViewMac::UpdatePreferredSize(const gfx::Size& new_size) {
  if (container_)
    container_->OnExtensionPreferredSizeChanged(this, new_size);
}

void ExtensionViewMac::RenderViewCreated() {
  // Do not allow webkit to draw scroll bars on views smaller than
  // the largest size view allowed.  The view will be resized to make
  // scroll bars unnecessary.  Scroll bars change the height of the
  // view, so not drawing them is necessary to avoid infinite resizing.
  gfx::Size largest_popup_size(
      CGSizeMake(ExtensionViewMac::kMaxWidth, ExtensionViewMac::kMaxHeight));
  extension_host_->DisableScrollbarsForSmallWindows(largest_popup_size);

  if (!pending_background_.empty() && render_view_host()->GetView()) {
    render_view_host()->GetView()->SetBackground(pending_background_);
    pending_background_.reset();
  }
}

void ExtensionViewMac::WindowFrameChanged() {
  if (render_view_host()->GetView())
    render_view_host()->GetView()->WindowFrameChanged();
}

void ExtensionViewMac::CreateWidgetHostView() {
  extension_host_->CreateRenderViewSoon();
}

void ExtensionViewMac::ShowIfCompletelyLoaded() {
  // We wait to show the ExtensionView until it has loaded, and the view has
  // actually been created. These can happen in different orders.
  if (extension_host_->did_stop_loading()) {
    [native_view() setHidden:NO];
    if (container_)
      container_->OnExtensionViewDidShow(this);
  }
}
