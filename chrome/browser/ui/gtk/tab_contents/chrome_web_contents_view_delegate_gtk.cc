// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/tab_contents/chrome_web_contents_view_delegate_gtk.h"

#include <map>

#include "base/lazy_instance.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents/render_view_context_menu_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents/web_drag_bookmark_handler_gtk.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/base/gtk/focus_store_gtk.h"
#include "ui/base/gtk/gtk_floating_container.h"

namespace {

const char kViewDelegateUserDataKey[] = "ChromeWebContentsViewDelegateGtk";

class ViewDelegateUserData : public base::SupportsUserData::Data {
 public:
  explicit ViewDelegateUserData(ChromeWebContentsViewDelegateGtk* view_delegate)
      : view_delegate_(view_delegate) {}
  virtual ~ViewDelegateUserData() {}
  ChromeWebContentsViewDelegateGtk* view_delegate() { return view_delegate_; }

 private:
  ChromeWebContentsViewDelegateGtk* view_delegate_;  // unowned
};

}  // namespace

ChromeWebContentsViewDelegateGtk* ChromeWebContentsViewDelegateGtk::GetFor(
    content::WebContents* web_contents) {
  ViewDelegateUserData* user_data = static_cast<ViewDelegateUserData*>(
      web_contents->GetUserData(&kViewDelegateUserDataKey));

  return user_data ? user_data->view_delegate() : NULL;
}

ChromeWebContentsViewDelegateGtk::ChromeWebContentsViewDelegateGtk(
    content::WebContents* web_contents)
    : floating_(gtk_floating_container_new()),
      constrained_window_(NULL),
      web_contents_(web_contents),
      expanded_container_(NULL),
      focus_store_(NULL) {
  gtk_widget_set_name(floating_.get(), "chrome-tab-contents-view");
  g_signal_connect(floating_.get(), "set-floating-position",
                   G_CALLBACK(OnSetFloatingPositionThunk), this);

  // Stash this in the WebContents.
  web_contents->SetUserData(&kViewDelegateUserDataKey,
                            new ViewDelegateUserData(this));
}

ChromeWebContentsViewDelegateGtk::~ChromeWebContentsViewDelegateGtk() {
  floating_.Destroy();
}

void ChromeWebContentsViewDelegateGtk::AttachConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  DCHECK(constrained_window_ == NULL);

  constrained_window_ = constrained_window;
  gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(floating_.get()),
                                      constrained_window->widget());
}

void ChromeWebContentsViewDelegateGtk::RemoveConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  DCHECK(constrained_window == constrained_window_);

  constrained_window_ = NULL;
  gtk_container_remove(GTK_CONTAINER(floating_.get()),
                       constrained_window->widget());
}

void ChromeWebContentsViewDelegateGtk::Initialize(
    GtkWidget* expanded_container, ui::FocusStoreGtk* focus_store) {
  expanded_container_ = expanded_container;
  focus_store_ = focus_store;
  // We install a chrome specific handler to intercept bookmark drags for the
  // bookmark manager/extension API.
  bookmark_handler_gtk_.reset(new WebDragBookmarkHandlerGtk);

  gtk_container_add(GTK_CONTAINER(floating_.get()), expanded_container);
  gtk_widget_show(floating_.get());
}

gfx::NativeView ChromeWebContentsViewDelegateGtk::GetNativeView() const {
  return floating_.get();
}

void ChromeWebContentsViewDelegateGtk::Focus() {
  if (!constrained_window_) {
    GtkWidget* widget = web_contents_->GetView()->GetContentNativeView();
    if (widget)
      gtk_widget_grab_focus(widget);
  }
}

gboolean ChromeWebContentsViewDelegateGtk::OnNativeViewFocusEvent(
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

  // Let the default WebContentsViewGtk::OnFocus() behaviour run.
  return FALSE;
}

void ChromeWebContentsViewDelegateGtk::ShowContextMenu(
    const content::ContextMenuParams& params,
    content::ContextMenuSourceType type) {
  // Find out the RenderWidgetHostView that corresponds to the render widget on
  // which this context menu is showed, so that we can retrieve the last mouse
  // down event on the render widget and use it as the timestamp of the
  // activation event to show the context menu.
  content::RenderWidgetHostView* view = NULL;
  if (params.custom_context.render_widget_id !=
      content::CustomContextMenuContext::kCurrentRenderWidget) {
    content::RenderWidgetHost* host =
        web_contents_->GetRenderProcessHost()->GetRenderWidgetHostByID(
            params.custom_context.render_widget_id);
    if (!host) {
      NOTREACHED();
      return;
    }
    view = host->GetView();
  } else {
    view = web_contents_->GetRenderWidgetHostView();
  }

  context_menu_.reset(
      new RenderViewContextMenuGtk(web_contents_, params, view));
  context_menu_->Init();

  // Don't show empty menus.
  if (context_menu_->menu_model().GetItemCount() == 0)
    return;

  gfx::Rect bounds;
  web_contents_->GetView()->GetContainerBounds(&bounds);
  gfx::Point point = bounds.origin();
  point.Offset(params.x, params.y);
  context_menu_->Popup(point);
}

content::WebDragDestDelegate*
    ChromeWebContentsViewDelegateGtk::GetDragDestDelegate() {
  return bookmark_handler_gtk_.get();
}

void ChromeWebContentsViewDelegateGtk::OnSetFloatingPosition(
    GtkWidget* floating_container, GtkAllocation* allocation) {
  if (!constrained_window_)
    return;

  // Place each ConstrainedWindowGtk in the center of the view.
  GtkWidget* widget = constrained_window_->widget();
  DCHECK(gtk_widget_get_parent(widget) == floating_.get());

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

namespace chrome {

content::WebContentsViewDelegate* CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return new ChromeWebContentsViewDelegateGtk(web_contents);
}

}  // namespace chrome
