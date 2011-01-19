// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents_view_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <algorithm>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_factory.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/render_view_context_menu_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/web_drag_dest_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_expanded_container.h"
#include "chrome/browser/ui/gtk/gtk_floating_container.h"
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/sad_tab_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents_drag_source.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "gfx/size.h"
#include "webkit/glue/webdropdata.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;

namespace {

// Called when the mouse leaves the widget. We notify our delegate.
gboolean OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event,
                       TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(
        tab_contents, gfx::Point(event->x_root, event->y_root), false);
  return FALSE;
}

// Called when the mouse moves within the widget. We notify our delegate.
gboolean OnMouseMove(GtkWidget* widget, GdkEventMotion* event,
                     TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(
        tab_contents, gfx::Point(event->x_root, event->y_root), true);
  return FALSE;
}

// See tab_contents_view_win.cc for discussion of mouse scroll zooming.
gboolean OnMouseScroll(GtkWidget* widget, GdkEventScroll* event,
                       TabContents* tab_contents) {
  if ((event->state & gtk_accelerator_get_default_mod_mask()) ==
      GDK_CONTROL_MASK) {
    if (event->direction == GDK_SCROLL_DOWN) {
      tab_contents->delegate()->ContentsZoomChange(false);
      return TRUE;
    } else if (event->direction == GDK_SCROLL_UP) {
      tab_contents->delegate()->ContentsZoomChange(true);
      return TRUE;
    }
  }

  return FALSE;
}

}  // namespace

// static
TabContentsView* TabContentsView::Create(TabContents* tab_contents) {
  return new TabContentsViewGtk(tab_contents);
}

TabContentsViewGtk::TabContentsViewGtk(TabContents* tab_contents)
    : TabContentsView(tab_contents),
      floating_(gtk_floating_container_new()),
      expanded_(gtk_expanded_container_new()),
      constrained_window_(NULL) {
  gtk_widget_set_name(expanded_, "chrome-tab-contents-view");
  g_signal_connect(expanded_, "size-allocate",
                   G_CALLBACK(OnSizeAllocateThunk), this);
  g_signal_connect(expanded_, "child-size-request",
                   G_CALLBACK(OnChildSizeRequestThunk), this);
  g_signal_connect(floating_.get(), "set-floating-position",
                   G_CALLBACK(OnSetFloatingPositionThunk), this);

  gtk_container_add(GTK_CONTAINER(floating_.get()), expanded_);
  gtk_widget_show(expanded_);
  gtk_widget_show(floating_.get());
  registrar_.Add(this, NotificationType::TAB_CONTENTS_CONNECTED,
                 Source<TabContents>(tab_contents));
  drag_source_.reset(new TabContentsDragSource(this));
}

TabContentsViewGtk::~TabContentsViewGtk() {
  floating_.Destroy();
}

void TabContentsViewGtk::AttachConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  DCHECK(constrained_window_ == NULL);

  constrained_window_ = constrained_window;
  gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(floating_.get()),
                                      constrained_window->widget());
}

void TabContentsViewGtk::RemoveConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  DCHECK(constrained_window == constrained_window_);

  constrained_window_ = NULL;
  gtk_container_remove(GTK_CONTAINER(floating_.get()),
                       constrained_window->widget());
}

void TabContentsViewGtk::CreateView(const gfx::Size& initial_size) {
  requested_size_ = initial_size;
}

RenderWidgetHostView* TabContentsViewGtk::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->view()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->view();
  }

  RenderWidgetHostViewGtk* view =
      new RenderWidgetHostViewGtk(render_widget_host);
  view->InitAsChild();
  gfx::NativeView content_view = view->native_view();
  g_signal_connect(content_view, "focus", G_CALLBACK(OnFocusThunk), this);
  g_signal_connect(content_view, "leave-notify-event",
                   G_CALLBACK(OnLeaveNotify), tab_contents());
  g_signal_connect(content_view, "motion-notify-event",
                   G_CALLBACK(OnMouseMove), tab_contents());
  g_signal_connect(content_view, "scroll-event",
                   G_CALLBACK(OnMouseScroll), tab_contents());
  gtk_widget_add_events(content_view, GDK_LEAVE_NOTIFY_MASK |
                        GDK_POINTER_MOTION_MASK);
  g_signal_connect(content_view, "button-press-event",
                   G_CALLBACK(OnMouseDownThunk), this);
  InsertIntoContentArea(content_view);

  // Renderer target DnD.
  drag_dest_.reset(new WebDragDestGtk(tab_contents(), content_view));

  return view;
}

gfx::NativeView TabContentsViewGtk::GetNativeView() const {
  return floating_.get();
}

gfx::NativeView TabContentsViewGtk::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = tab_contents()->GetRenderWidgetHostView();
  if (!rwhv)
    return NULL;
  return rwhv->GetNativeView();
}

gfx::NativeWindow TabContentsViewGtk::GetTopLevelNativeWindow() const {
  GtkWidget* window = gtk_widget_get_ancestor(GetNativeView(), GTK_TYPE_WINDOW);
  return window ? GTK_WINDOW(window) : NULL;
}

void TabContentsViewGtk::GetContainerBounds(gfx::Rect* out) const {
  // This is used for positioning the download shelf arrow animation,
  // as well as sizing some other widgets in Windows.  In GTK the size is
  // managed for us, so it appears to be only used for the download shelf
  // animation.
  int x = 0;
  int y = 0;
  if (expanded_->window)
    gdk_window_get_origin(expanded_->window, &x, &y);
  out->SetRect(x + expanded_->allocation.x, y + expanded_->allocation.y,
               requested_size_.width(), requested_size_.height());
}

void TabContentsViewGtk::SetPageTitle(const std::wstring& title) {
  // Set the window name to include the page title so it's easier to spot
  // when debugging (e.g. via xwininfo -tree).
  gfx::NativeView content_view = GetContentNativeView();
  if (content_view && content_view->window)
    gdk_window_set_title(content_view->window, WideToUTF8(title).c_str());
}

void TabContentsViewGtk::OnTabCrashed(base::TerminationStatus status,
                                      int error_code) {
  if (tab_contents() != NULL && !sad_tab_.get()) {
    sad_tab_.reset(new SadTabGtk(
        tab_contents(),
        status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ?
        SadTabGtk::KILLED : SadTabGtk::CRASHED));
    InsertIntoContentArea(sad_tab_->widget());
    gtk_widget_show(sad_tab_->widget());
  }
}

void TabContentsViewGtk::SizeContents(const gfx::Size& size) {
  // We don't need to manually set the size of of widgets in GTK+, but we do
  // need to pass the sizing information on to the RWHV which will pass the
  // sizing information on to the renderer.
  requested_size_ = size;
  RenderWidgetHostView* rwhv = tab_contents()->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
}

void TabContentsViewGtk::Focus() {
  if (tab_contents()->showing_interstitial_page()) {
    tab_contents()->interstitial_page()->Focus();
  } else if (!constrained_window_) {
    GtkWidget* widget = GetContentNativeView();
    if (widget)
      gtk_widget_grab_focus(widget);
  }
}

void TabContentsViewGtk::SetInitialFocus() {
  if (tab_contents()->FocusLocationBarByDefault())
    tab_contents()->SetFocusToLocationBar(false);
  else
    Focus();
}

void TabContentsViewGtk::StoreFocus() {
  focus_store_.Store(GetNativeView());
}

void TabContentsViewGtk::RestoreFocus() {
  if (focus_store_.widget())
    gtk_widget_grab_focus(focus_store_.widget());
  else
    SetInitialFocus();
}

void TabContentsViewGtk::GetViewBounds(gfx::Rect* out) const {
  if (!floating_->window) {
    out->SetRect(0, 0, requested_size_.width(), requested_size_.height());
    return;
  }
  int x = 0, y = 0, w, h;
  gdk_window_get_geometry(floating_->window, &x, &y, &w, &h, NULL);
  out->SetRect(x, y, w, h);
}

void TabContentsViewGtk::SetFocusedWidget(GtkWidget* widget) {
  focus_store_.SetWidget(widget);
}

void TabContentsViewGtk::UpdateDragCursor(WebDragOperation operation) {
  drag_dest_->UpdateDragStatus(operation);
}

void TabContentsViewGtk::GotFocus() {
  // This is only used in the views FocusManager stuff but it bleeds through
  // all subclasses. http://crbug.com/21875
}

// This is called when we the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void TabContentsViewGtk::TakeFocus(bool reverse) {
  if (!tab_contents()->delegate()->TakeFocus(reverse)) {
    gtk_widget_child_focus(GTK_WIDGET(GetTopLevelNativeWindow()),
        reverse ? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD);
  }
}

void TabContentsViewGtk::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::TAB_CONTENTS_CONNECTED: {
      // No need to remove the SadTabGtk's widget from the container since
      // the new RenderWidgetHostViewGtk instance already removed all the
      // vbox's children.
      sad_tab_.reset();
      break;
    }
    default:
      NOTREACHED() << "Got a notification we didn't register for.";
      break;
  }
}

void TabContentsViewGtk::ShowContextMenu(const ContextMenuParams& params) {
  context_menu_.reset(new RenderViewContextMenuGtk(tab_contents(), params,
                                                   last_mouse_down_.time));
  context_menu_->Init();

  gfx::Rect bounds;
  GetContainerBounds(&bounds);
  gfx::Point point = bounds.origin();
  point.Offset(params.x, params.y);
  context_menu_->Popup(point);
}

void TabContentsViewGtk::ShowPopupMenu(const gfx::Rect& bounds,
                                       int item_height,
                                       double item_font_size,
                                       int selected_item,
                                       const std::vector<WebMenuItem>& items,
                                       bool right_aligned) {
  // We are not using external popup menus on Linux, they are rendered by
  // WebKit.
  NOTREACHED();
}

// Render view DnD -------------------------------------------------------------

void TabContentsViewGtk::StartDragging(const WebDropData& drop_data,
                                       WebDragOperationsMask ops,
                                       const SkBitmap& image,
                                       const gfx::Point& image_offset) {
  DCHECK(GetContentNativeView());
  drag_source_->StartDragging(drop_data, ops, &last_mouse_down_, image,
                              image_offset);
}

// -----------------------------------------------------------------------------

void TabContentsViewGtk::InsertIntoContentArea(GtkWidget* widget) {
  gtk_container_add(GTK_CONTAINER(expanded_), widget);
}

// Called when the content view gtk widget is tabbed to, or after the call to
// gtk_widget_child_focus() in TakeFocus(). We return true
// and grab focus if we don't have it. The call to
// FocusThroughTabTraversal(bool) forwards the "move focus forward" effect to
// webkit.
gboolean TabContentsViewGtk::OnFocus(GtkWidget* widget,
                                     GtkDirectionType focus) {
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
    return FALSE;
  }

  // If we already have focus, let the next widget have a shot at it. We will
  // reach this situation after the call to gtk_widget_child_focus() in
  // TakeFocus().
  if (gtk_widget_is_focus(widget))
    return FALSE;

  gtk_widget_grab_focus(widget);
  bool reverse = focus == GTK_DIR_TAB_BACKWARD;
  tab_contents()->FocusThroughTabTraversal(reverse);
  return TRUE;
}

gboolean TabContentsViewGtk::OnMouseDown(GtkWidget* widget,
                                         GdkEventButton* event) {
  last_mouse_down_ = *event;
  return FALSE;
}

void TabContentsViewGtk::OnChildSizeRequest(GtkWidget* widget,
                                            GtkWidget* child,
                                            GtkRequisition* requisition) {
  if (tab_contents()->delegate()) {
    requisition->height +=
        tab_contents()->delegate()->GetExtraRenderViewHeight();
  }
}

void TabContentsViewGtk::OnSizeAllocate(GtkWidget* widget,
                                        GtkAllocation* allocation) {
  int width = allocation->width;
  int height = allocation->height;
  // |delegate()| can be NULL here during browser teardown.
  if (tab_contents()->delegate())
    height += tab_contents()->delegate()->GetExtraRenderViewHeight();
  gfx::Size size(width, height);
  requested_size_ = size;

  // We manually tell our RWHV to resize the renderer content.  This avoids
  // spurious resizes from GTK+.
  RenderWidgetHostView* rwhv = tab_contents()->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
  if (tab_contents()->interstitial_page())
    tab_contents()->interstitial_page()->SetSize(size);
}

void TabContentsViewGtk::OnSetFloatingPosition(
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
