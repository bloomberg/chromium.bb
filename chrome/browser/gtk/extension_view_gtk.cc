// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/extension_view_gtk.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"

ExtensionViewGtk::ExtensionViewGtk(ExtensionHost* extension_host)
    : extension_host_(extension_host),
      render_widget_host_view_(NULL) {
  CreateWidgetHostView();
}

gfx::NativeView ExtensionViewGtk::native_view() {
  return render_widget_host_view_->native_view();
}

RenderViewHost* ExtensionViewGtk::render_view_host() const {
  return extension_host_->render_view_host();
}

void ExtensionViewGtk::CreateWidgetHostView() {
  DCHECK(!render_widget_host_view_);
  render_widget_host_view_ = new RenderWidgetHostViewGtk(render_view_host());
  render_widget_host_view_->InitAsChild();

  extension_host_->CreateRenderView(render_widget_host_view_);
}
