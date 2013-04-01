// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/ui/gtk/extensions/extension_popup_gtk.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "extensions/common/view_type.h"

ExtensionViewGtk::ExtensionViewGtk(extensions::ExtensionHost* extension_host,
                                   Browser* browser)
    : browser_(browser),
      extension_host_(extension_host),
      container_(NULL) {
}

void ExtensionViewGtk::Init() {
  CreateWidgetHostView();
}

gfx::NativeView ExtensionViewGtk::native_view() {
  return extension_host_->host_contents()->GetView()->GetNativeView();
}

content::RenderViewHost* ExtensionViewGtk::render_view_host() const {
  return extension_host_->render_view_host();
}

void ExtensionViewGtk::SetBackground(const SkBitmap& background) {
  if (render_view_host()->IsRenderViewLive() && render_view_host()->GetView()) {
    render_view_host()->GetView()->SetBackground(background);
  } else {
    pending_background_ = background;
  }
}

void ExtensionViewGtk::ResizeDueToAutoResize(const gfx::Size& new_size) {
  if (container_)
    container_->OnExtensionSizeChanged(this, new_size);
}

void ExtensionViewGtk::CreateWidgetHostView() {
  extension_host_->CreateRenderViewSoon();
}

void ExtensionViewGtk::RenderViewCreated() {
  if (!pending_background_.empty() && render_view_host()->GetView()) {
    render_view_host()->GetView()->SetBackground(pending_background_);
    pending_background_.reset();
  }

  extensions::ViewType host_type = extension_host_->extension_host_type();
  if (host_type == extensions::VIEW_TYPE_EXTENSION_POPUP) {
    gfx::Size min_size(ExtensionPopupGtk::kMinWidth,
                       ExtensionPopupGtk::kMinHeight);
    gfx::Size max_size(ExtensionPopupGtk::kMaxWidth,
                       ExtensionPopupGtk::kMaxHeight);
    render_view_host()->EnableAutoResize(min_size, max_size);
  }
}
