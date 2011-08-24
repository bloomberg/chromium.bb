// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_views.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gtk_native_view_id_manager.h"
#include "views/views_delegate.h"
#include "views/widget/native_widget_gtk.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/x11/WebScreenInfoFactory.h"

namespace {

// TODO(oshima): This is a copy from RenderWidgetHostViewGtk. Replace this
// with gdk-less implementation.
void GetScreenInfoFromNativeWindow(
    GdkWindow* gdk_window, WebKit::WebScreenInfo* results) {
  GdkScreen* screen = gdk_drawable_get_screen(gdk_window);
  *results = WebKit::WebScreenInfoFactory::screenInfo(
      gdk_x11_drawable_get_xdisplay(gdk_window),
      gdk_x11_screen_get_screen_number(screen));

  // TODO(tony): We should move this code into WebScreenInfoFactory.
  int monitor_number = gdk_screen_get_monitor_at_window(screen, gdk_window);
  GdkRectangle monitor_rect;
  gdk_screen_get_monitor_geometry(screen, monitor_number, &monitor_rect);
  results->rect = WebKit::WebRect(monitor_rect.x, monitor_rect.y,
                                  monitor_rect.width, monitor_rect.height);
  // TODO(tony): Should we query _NET_WORKAREA to get the workarea?
  results->availableRect = results->rect;
}

}  // namespace

void RenderWidgetHostViewViews::UpdateCursor(const WebCursor& cursor) {
  // Optimize the common case, where the cursor hasn't changed.
  // However, we can switch between different pixmaps, so only on the
  // non-pixmap branch.
  if (current_cursor_.GetCursorType() != GDK_CURSOR_IS_PIXMAP &&
      current_cursor_.GetCursorType() == cursor.GetCursorType()) {
    return;
  }

  current_cursor_ = cursor;
  ShowCurrentCursor();
}

void RenderWidgetHostViewViews::CreatePluginContainer(
    gfx::PluginWindowHandle id) {
  // TODO(anicolao): plugin_container_manager_.CreatePluginContainer(id);
}

void RenderWidgetHostViewViews::DestroyPluginContainer(
    gfx::PluginWindowHandle id) {
  // TODO(anicolao): plugin_container_manager_.DestroyPluginContainer(id);
}

void RenderWidgetHostViewViews::GetScreenInfo(WebKit::WebScreenInfo* results) {
  views::Widget* widget = GetWidget() ? GetWidget()->GetTopLevelWidget() : NULL;
  if (widget)
    GetScreenInfoFromNativeWindow(widget->GetNativeView()->window, results);
}

gfx::Rect RenderWidgetHostViewViews::GetRootWindowBounds() {
  views::Widget* widget = GetWidget() ? GetWidget()->GetTopLevelWidget() : NULL;
  return widget ? widget->GetWindowScreenBounds() : gfx::Rect();
}

void RenderWidgetHostViewViews::AcceleratedCompositingActivated(
    bool activated) {
  // TODO(anicolao): figure out if we need something here
  if (activated)
    NOTIMPLEMENTED();
}

#if !defined(TOUCH_UI)
gfx::PluginWindowHandle RenderWidgetHostViewViews::GetCompositingSurface() {
  // TODO(oshima): The original implementation was broken as
  // GtkNativeViewManager doesn't know about NativeWidgetGtk. Figure
  // out if this makes sense without compositor. If it does, then find
  // out the right way to handle.
  NOTIMPLEMENTED();
  return gfx::kNullPluginWindow;
}
#endif

gfx::NativeView RenderWidgetHostViewViews::GetInnerNativeView() const {
  const views::View* view = NULL;
  if (views::ViewsDelegate::views_delegate)
    view = views::ViewsDelegate::views_delegate->GetDefaultParentView();
  if (!view)
    view = this;

  // TODO(sad): Ideally this function should be equivalent to GetNativeView, and
  // NativeWidgetGtk-specific function call should not be necessary.
  const views::Widget* widget = view->GetWidget();
  const views::NativeWidget* native = widget ? widget->native_widget() : NULL;
  return native ? static_cast<const views::NativeWidgetGtk*>(native)->
      window_contents() : NULL;
}

void RenderWidgetHostViewViews::ShowCurrentCursor() {
  // The widget may not have a window. If that's the case, abort mission. This
  // is the same issue as that explained above in Paint().
  if (!GetInnerNativeView() || !GetInnerNativeView()->window)
    return;

  native_cursor_ = current_cursor_.GetNativeCursor();
}

#if defined(TOUCH_UI)
// static
void RenderWidgetHostView::GetDefaultScreenInfo(
    WebKit::WebScreenInfo* results) {
  GdkWindow* gdk_window =
      gdk_display_get_default_group(gdk_display_get_default());
  GetScreenInfoFromNativeWindow(gdk_window, results);
}
#endif
