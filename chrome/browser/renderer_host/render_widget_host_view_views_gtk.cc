// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_views.h"

#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gtk_native_view_id_manager.h"
#include "views/widget/native_widget_gtk.h"

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

void RenderWidgetHostViewViews::AcceleratedCompositingActivated(
    bool activated) {
  // TODO(anicolao): figure out if we need something here
  if (activated)
    NOTIMPLEMENTED();
}

gfx::PluginWindowHandle RenderWidgetHostViewViews::GetCompositingSurface() {
  GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
  gfx::PluginWindowHandle surface = gfx::kNullPluginWindow;
  gfx::NativeViewId view_id = gfx::IdFromNativeView(GetInnerNativeView());

  if (!manager->GetXIDForId(&surface, view_id)) {
    DLOG(ERROR) << "Can't find XID for view id " << view_id;
  }
  return surface;
}

gfx::NativeView RenderWidgetHostViewViews::GetInnerNativeView() const {
  // TODO(sad): Ideally this function should be equivalent to GetNativeView, and
  // NativeWidgetGtk-specific function call should not be necessary.
  const views::Widget* widget = GetWidget();
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

