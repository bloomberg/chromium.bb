// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/chrome_tab_contents_view_wrapper_gtk.h"

#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/tab_contents/render_view_context_menu_gtk.h"
#include "chrome/browser/tab_contents/tab_contents_view_gtk.h"
#include "chrome/browser/tab_contents/web_drag_bookmark_handler_gtk.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view_gtk.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "ui/base/gtk/gtk_floating_container.h"

ChromeTabContentsViewWrapperGtk::ChromeTabContentsViewWrapperGtk()
    : floating_(gtk_floating_container_new()),
      view_(NULL),
      constrained_window_(NULL) {
  gtk_widget_set_name(floating_.get(), "chrome-tab-contents-wrapper-view");
  g_signal_connect(floating_.get(), "set-floating-position",
                   G_CALLBACK(OnSetFloatingPositionThunk), this);
}

ChromeTabContentsViewWrapperGtk::~ChromeTabContentsViewWrapperGtk() {
  floating_.Destroy();
}

void ChromeTabContentsViewWrapperGtk::AttachConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  DCHECK(constrained_window_ == NULL);

  constrained_window_ = constrained_window;
  gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(floating_.get()),
                                      constrained_window->widget());
}

void ChromeTabContentsViewWrapperGtk::RemoveConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  DCHECK(constrained_window == constrained_window_);

  constrained_window_ = NULL;
  gtk_container_remove(GTK_CONTAINER(floating_.get()),
                       constrained_window->widget());
}

void ChromeTabContentsViewWrapperGtk::WrapView(TabContentsViewGtk* view) {
  view_ = view;

  gtk_container_add(GTK_CONTAINER(floating_.get()),
                    view_->expanded_container());
  gtk_widget_show(floating_.get());
}

gfx::NativeView ChromeTabContentsViewWrapperGtk::GetNativeView() const {
  return floating_.get();
}

void ChromeTabContentsViewWrapperGtk::OnCreateViewForWidget() {
  // We install a chrome specific handler to intercept bookmark drags for the
  // bookmark manager/extension API.
  bookmark_handler_gtk_.reset(new WebDragBookmarkHandlerGtk);
  view_->SetDragDestDelegate(bookmark_handler_gtk_.get());
}

void ChromeTabContentsViewWrapperGtk::Focus() {
  if (!constrained_window_) {
    GtkWidget* widget = view_->GetContentNativeView();
    if (widget)
      gtk_widget_grab_focus(widget);
  }
}

gboolean ChromeTabContentsViewWrapperGtk::OnNativeViewFocusEvent(
    GtkWidget* widget,
    GtkDirectionType type,
    gboolean* return_value) {
  // If we are showing a constrained window, don't allow the native view to take
  // focus.
  if (constrained_window_) {
    // If we return false, it will revert to the default handler, which will
    // take focus. We don't want that. But if we return true, the event will
    // stop being propagated, leaving focus wherever it is currently. That is
    // also bad. So we return false to let the default handler run, but take
    // focus first so as to trick it into thinking the view was already focused
    // and allowing the event to propagate.
    gtk_widget_grab_focus(widget);
    *return_value = FALSE;
    return TRUE;
  }

  // Let the default TabContentsViewGtk::OnFocus() behaviour run.
  return FALSE;
}

void ChromeTabContentsViewWrapperGtk::ShowContextMenu(
    const ContextMenuParams& params) {
  // Find out the RenderWidgetHostView that corresponds to the render widget on
  // which this context menu is showed, so that we can retrieve the last mouse
  // down event on the render widget and use it as the timestamp of the
  // activation event to show the context menu.
  RenderWidgetHostView* view = NULL;
  if (params.custom_context.render_widget_id !=
      webkit_glue::CustomContextMenuContext::kCurrentRenderWidget) {
    IPC::Channel::Listener* listener =
        view_->tab_contents()->render_view_host()->process()->GetListenerByID(
            params.custom_context.render_widget_id);
    if (!listener) {
      NOTREACHED();
      return;
    }
    view = static_cast<RenderWidgetHost*>(listener)->view();
  } else {
    view = view_->tab_contents()->GetRenderWidgetHostView();
  }
  RenderWidgetHostViewGtk* view_gtk =
      static_cast<RenderWidgetHostViewGtk*>(view);
  if (!view_gtk)
    return;

  context_menu_.reset(new RenderViewContextMenuGtk(
      view_->tab_contents(), params, view_gtk->last_mouse_down() ?
      view_gtk->last_mouse_down()->time : GDK_CURRENT_TIME));
  context_menu_->Init();

  gfx::Rect bounds;
  view_->GetContainerBounds(&bounds);
  gfx::Point point = bounds.origin();
  point.Offset(params.x, params.y);
  context_menu_->Popup(point);
}

void ChromeTabContentsViewWrapperGtk::OnSetFloatingPosition(
    GtkWidget* floating_container, GtkAllocation* allocation) {
  if (!constrained_window_)
    return;

  // Place each ConstrainedWindow in the center of the view.
  GtkWidget* widget = constrained_window_->widget();
  DCHECK(widget->parent == floating_.get());

  GtkRequisition requisition;
  gtk_widget_size_request(widget, &requisition);

  GValue value = { 0, };
  g_value_init(&value, G_TYPE_INT);

  int child_x = std::max((allocation->width - requisition.width) / 2, 0);
  g_value_set_int(&value, child_x);
  gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                   widget, "x", &value);

  int child_y = std::max((allocation->height - requisition.height) / 2, 0);
  g_value_set_int(&value, child_y);
  gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                   widget, "y", &value);
  g_value_unset(&value);
}
