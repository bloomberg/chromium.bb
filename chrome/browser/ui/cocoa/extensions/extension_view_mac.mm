// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/common/view_type.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

// The minimum/maximum dimensions of the popup.
const CGFloat ExtensionViewMac::kMinWidth = 25.0;
const CGFloat ExtensionViewMac::kMinHeight = 25.0;
const CGFloat ExtensionViewMac::kMaxWidth = 800.0;
const CGFloat ExtensionViewMac::kMaxHeight = 600.0;

ExtensionViewMac::ExtensionViewMac(extensions::ExtensionHost* extension_host,
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

content::RenderViewHost* ExtensionViewMac::render_view_host() const {
  return extension_host_->render_view_host();
}

void ExtensionViewMac::DidStopLoading() {
  ShowIfCompletelyLoaded();
}

void ExtensionViewMac::ResizeDueToAutoResize(const gfx::Size& new_size) {
  if (container_)
    container_->OnExtensionSizeChanged(this, new_size);
}

void ExtensionViewMac::RenderViewCreated() {
  chrome::ViewType host_type = extension_host_->extension_host_type();
  if (host_type == chrome::VIEW_TYPE_EXTENSION_POPUP) {
    gfx::Size min_size(ExtensionViewMac::kMinWidth,
                       ExtensionViewMac::kMinHeight);
    gfx::Size max_size(ExtensionViewMac::kMaxWidth,
                       ExtensionViewMac::kMaxHeight);
    render_view_host()->EnableAutoResize(min_size, max_size);
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
