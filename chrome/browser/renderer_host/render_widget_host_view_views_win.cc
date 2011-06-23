// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_views.h"

#include "base/logging.h"

void RenderWidgetHostViewViews::UpdateCursor(const WebCursor& cursor) {
}

void RenderWidgetHostViewViews::WillWmDestroy() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewViews::ShowCompositorHostWindow(bool show) {
  NOTIMPLEMENTED();
}

gfx::PluginWindowHandle RenderWidgetHostViewViews::GetCompositingSurface() {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::NativeView RenderWidgetHostViewViews::GetInnerNativeView() const {
  NOTIMPLEMENTED();
  // TODO(beng): Figure out what to do here for Windows/v.o.v.
  return NULL;
}

void RenderWidgetHostViewViews::ShowCurrentCursor() {
  NOTIMPLEMENTED();
}
