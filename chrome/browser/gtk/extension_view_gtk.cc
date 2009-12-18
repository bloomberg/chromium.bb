// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/extension_view_gtk.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"

namespace {

// The minimum/maximum dimensions of the extension view.
// The minimum is just a little larger than the size of a browser action button.
// The maximum is an arbitrary number that should be smaller than most screens.
const int kMinWidth = 25;
const int kMinHeight = 25;
const int kMaxWidth = 800;
const int kMaxHeight = 600;

}  // namespace

ExtensionViewGtk::ExtensionViewGtk(ExtensionHost* extension_host,
                                   Browser* browser)
    : browser_(browser),
      extension_host_(extension_host),
      render_widget_host_view_(NULL) {
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
  int width = std::max(kMinWidth, std::min(kMaxWidth, new_size.width()));
  int height = std::max(kMinHeight, std::min(kMaxHeight, new_size.height()));

  render_widget_host_view_->SetSize(gfx::Size(width, height));
  gtk_widget_set_size_request(native_view(), width, height);
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
}
