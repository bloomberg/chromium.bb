// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_views.h"

#include "base/logging.h"
#if !defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_win.h"
#endif
#include "views/widget/widget.h"

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
#if !defined(USE_AURA)
  if (views::Widget::IsPureViews())
#endif
    return new RenderWidgetHostViewViews(widget);
#if !defined(USE_AURA)
  return new RenderWidgetHostViewWin(widget);
#endif
}

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

void RenderWidgetHostViewViews::ShowCurrentCursor() {
  NOTIMPLEMENTED();
}
