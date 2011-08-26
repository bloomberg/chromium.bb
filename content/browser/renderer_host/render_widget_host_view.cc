// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"

#if defined(OS_MACOSX)
#include "third_party/WebKit/Source/WebKit/chromium/public/mac/WebScreenInfoFactory.h"
#endif

#if defined(TOUCH_UI)
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "content/browser/renderer_host/gtk_window_utils.h"
#endif

// TODO(jam): move this to render_widget_host_view_mac.mm when it moves to
// content.
#if defined(OS_MACOSX)
// static
void RenderWidgetHostView::GetDefaultScreenInfo(
    WebKit::WebScreenInfo* results) {
  *results = WebKit::WebScreenInfoFactory::screenInfo(NULL);
}
#endif

// TODO(erg): move this to render_widget_host_view_views_gtk.cc when if it
// moves to content.
#if defined(TOUCH_UI)
// static
void RenderWidgetHostView::GetDefaultScreenInfo(
    WebKit::WebScreenInfo* results) {
  GdkWindow* gdk_window =
      gdk_display_get_default_group(gdk_display_get_default());
  content::GetScreenInfoFromNativeWindow(gdk_window, results);
}
#endif

RenderWidgetHostView::~RenderWidgetHostView() {}

void RenderWidgetHostView::SetBackground(const SkBitmap& background) {
  background_ = background;
}
