// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_views.h"

#include "base/logging.h"
#include "views/widget/widget.h"

void RenderWidgetHostViewViews::UpdateCursor(const WebCursor& cursor) {
}

#if !defined(USE_AURA)
void RenderWidgetHostViewViews::WillWmDestroy() {
  NOTIMPLEMENTED();
}
#endif

gfx::PluginWindowHandle RenderWidgetHostViewViews::GetCompositingSurface() {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewViews::ShowCurrentCursor() {
  NOTIMPLEMENTED();
}
