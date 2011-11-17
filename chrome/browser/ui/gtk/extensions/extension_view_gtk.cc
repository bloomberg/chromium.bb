// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/ui/gtk/extensions/extension_popup_gtk.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"

ExtensionViewGtk::ExtensionViewGtk(ExtensionHost* extension_host,
                                   Browser* browser)
    : browser_(browser),
      extension_host_(extension_host),
      container_(NULL) {
}

void ExtensionViewGtk::Init() {
  CreateWidgetHostView();
}

gfx::NativeView ExtensionViewGtk::native_view() {
  return extension_host_->host_contents()->view()->GetNativeView();
}

RenderViewHost* ExtensionViewGtk::render_view_host() const {
  return extension_host_->render_view_host();
}

void ExtensionViewGtk::SetBackground(const SkBitmap& background) {
  if (render_view_host()->IsRenderViewLive() && render_view_host()->view()) {
    render_view_host()->view()->SetBackground(background);
  } else {
    pending_background_ = background;
  }
}

void ExtensionViewGtk::UpdatePreferredSize(const gfx::Size& new_size) {
  if (container_)
    container_->OnExtensionPreferredSizeChanged(this, new_size);
}

void ExtensionViewGtk::CreateWidgetHostView() {
  extension_host_->CreateRenderViewSoon();
}

void ExtensionViewGtk::RenderViewCreated() {
  if (!pending_background_.empty() && render_view_host()->view()) {
    render_view_host()->view()->SetBackground(pending_background_);
    pending_background_.reset();
  }

  // Tell the renderer not to draw scrollbars in popups unless the
  // popups are at the maximum allowed size.
  gfx::Size largest_popup_size(ExtensionPopupGtk::kMaxWidth,
                               ExtensionPopupGtk::kMaxHeight);
  extension_host_->DisableScrollbarsForSmallWindows(largest_popup_size);
}
