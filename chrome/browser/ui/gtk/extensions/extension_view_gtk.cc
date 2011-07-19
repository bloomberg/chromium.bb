// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/ui/gtk/extensions/extension_popup_gtk.h"
#include "content/browser/renderer_host/render_view_host.h"

ExtensionViewGtk::ExtensionViewGtk(ExtensionHost* extension_host,
                                   Browser* browser)
    : browser_(browser),
      extension_host_(extension_host),
      render_widget_host_view_(NULL),
      container_(NULL) {
}

void ExtensionViewGtk::Init() {
  CreateWidgetHostView();
}

gfx::NativeView ExtensionViewGtk::native_view() {
  return render_widget_host_view_->native_view();
}

RenderViewHost* ExtensionViewGtk::render_view_host() const {
  return extension_host_->render_view_host();
}

void ExtensionViewGtk::SetBackground(const SkBitmap& background) {
  if (render_view_host()->IsRenderViewLive()) {
    render_widget_host_view_->SetBackground(background);
  } else {
    pending_background_ = background;
  }
}

void ExtensionViewGtk::UpdatePreferredSize(const gfx::Size& new_size) {
  if (container_)
    container_->OnExtensionPreferredSizeChanged(this, new_size);
}

void ExtensionViewGtk::CreateWidgetHostView() {
  DCHECK(!render_widget_host_view_);
  render_widget_host_view_ = new RenderWidgetHostViewGtk(render_view_host());
  render_widget_host_view_->InitAsChild();

  extension_host_->CreateRenderViewSoon(render_widget_host_view_);
}

void ExtensionViewGtk::RenderViewCreated() {
  if (!pending_background_.empty() && render_view_host()->view()) {
    render_widget_host_view_->SetBackground(pending_background_);
    pending_background_.reset();
  }

  // Tell the renderer not to draw scrollbars in popups unless the
  // popups are at the maximum allowed size.
  gfx::Size largest_popup_size(ExtensionPopupGtk::kMaxWidth,
                               ExtensionPopupGtk::kMaxHeight);
  extension_host_->DisableScrollbarsForSmallWindows(largest_popup_size);
}
