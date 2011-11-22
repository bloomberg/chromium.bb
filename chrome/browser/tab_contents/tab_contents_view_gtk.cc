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
#include "chrome/browser/tab_contents/tab_contents_view_wrapper_gtk.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_view_gtk.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/tab_contents/web_drag_dest_gtk.h"
#include "content/browser/tab_contents/web_drag_source_gtk.h"
#include "ui/base/gtk/gtk_expanded_container.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
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

// See tab_contents_view_views.cc for discussion of mouse scroll zooming.
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

TabContentsViewGtk::TabContentsViewGtk(TabContents* tab_contents,
                                       TabContentsViewWrapperGtk* view_wrapper)
    : tab_contents_(tab_contents),
      expanded_(gtk_expanded_container_new()),
      view_wrapper_(view_wrapper),
      overlaid_view_(NULL) {
  gtk_widget_set_name(expanded_.get(), "chrome-tab-contents-view");
  g_signal_connect(expanded_.get(), "size-allocate",
                   G_CALLBACK(OnSizeAllocateThunk), this);
  g_signal_connect(expanded_.get(), "child-size-request",
                   G_CALLBACK(OnChildSizeRequestThunk), this);

  gtk_widget_show(expanded_.get());
  drag_source_.reset(new content::WebDragSourceGtk(tab_contents));

  if (view_wrapper_.get())
    view_wrapper_->WrapView(this);
}

TabContentsViewGtk::~TabContentsViewGtk() {
  expanded_.Destroy();
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
                   G_CALLBACK(OnLeaveNotify), tab_contents_);
  g_signal_connect(content_view, "motion-notify-event",
                   G_CALLBACK(OnMouseMove), tab_contents_);
  g_signal_connect(content_view, "scroll-event",
                   G_CALLBACK(OnMouseScroll), tab_contents_);
  gtk_widget_add_events(content_view, GDK_LEAVE_NOTIFY_MASK |
                        GDK_POINTER_MOTION_MASK);
  InsertIntoContentArea(content_view);

  // Renderer target DnD.
  drag_dest_.reset(new content::WebDragDestGtk(tab_contents_, content_view));

  if (view_wrapper_.get())
    view_wrapper_->OnCreateViewForWidget();

  return view;
}

gfx::NativeView TabContentsViewGtk::GetNativeView() const {
  if (view_wrapper_.get())
    return view_wrapper_->GetNativeView();

  return expanded_.get();
}

gfx::NativeView TabContentsViewGtk::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = tab_contents_->GetRenderWidgetHostView();
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
  GdkWindow* expanded_window = gtk_widget_get_window(expanded_.get());
  if (expanded_window)
    gdk_window_get_origin(expanded_window, &x, &y);
  out->SetRect(x + expanded_->allocation.x, y + expanded_->allocation.y,
               requested_size_.width(), requested_size_.height());
}

void TabContentsViewGtk::SetPageTitle(const string16& title) {
  // Set the window name to include the page title so it's easier to spot
  // when debugging (e.g. via xwininfo -tree).
  gfx::NativeView content_view = GetContentNativeView();
  if (content_view) {
    GdkWindow* content_window = gtk_widget_get_window(content_view);
    if (content_window) {
      gdk_window_set_title(content_window, UTF16ToUTF8(title).c_str());
    }
  }
}

void TabContentsViewGtk::OnTabCrashed(base::TerminationStatus status,
                                      int error_code) {
}

void TabContentsViewGtk::SizeContents(const gfx::Size& size) {
  // We don't need to manually set the size of of widgets in GTK+, but we do
  // need to pass the sizing information on to the RWHV which will pass the
  // sizing information on to the renderer.
  requested_size_ = size;
  RenderWidgetHostView* rwhv = tab_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
}

void TabContentsViewGtk::RenderViewCreated(RenderViewHost* host) {
}

void TabContentsViewGtk::Focus() {
  if (tab_contents_->showing_interstitial_page()) {
    tab_contents_->interstitial_page()->Focus();
  } else if (wrapper()) {
    wrapper()->Focus();
  }
}

void TabContentsViewGtk::SetInitialFocus() {
  if (tab_contents_->FocusLocationBarByDefault())
    tab_contents_->SetFocusToLocationBar(false);
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

bool TabContentsViewGtk::IsDoingDrag() const {
  return false;
}

void TabContentsViewGtk::CancelDragAndCloseTab() {
}

bool TabContentsViewGtk::IsEventTracking() const {
  return false;
}

void TabContentsViewGtk::CloseTabAfterEventTracking() {
}

void TabContentsViewGtk::GetViewBounds(gfx::Rect* out) const {
  GdkWindow* window = gtk_widget_get_window(GetNativeView());
  if (!window) {
    out->SetRect(0, 0, requested_size_.width(), requested_size_.height());
    return;
  }
  int x = 0, y = 0, w, h;
  gdk_window_get_geometry(window, &x, &y, &w, &h, NULL);
  out->SetRect(x, y, w, h);
}

void TabContentsViewGtk::InstallOverlayView(gfx::NativeView view) {
  DCHECK(!overlaid_view_);
  overlaid_view_ = view;
  InsertIntoContentArea(view);
  gtk_widget_show(view);
}

void TabContentsViewGtk::RemoveOverlayView() {
  DCHECK(overlaid_view_);
  gtk_container_remove(GTK_CONTAINER(expanded_.get()), overlaid_view_);
  overlaid_view_ = NULL;
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

// This is called when the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void TabContentsViewGtk::TakeFocus(bool reverse) {
  if (!tab_contents_->delegate())
    return;
  if (!tab_contents_->delegate()->TakeFocus(reverse)) {
    gtk_widget_child_focus(GTK_WIDGET(GetTopLevelNativeWindow()),
        reverse ? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD);
  }
}

void TabContentsViewGtk::SetDragDestDelegate(
    content::WebDragDestDelegate* delegate) {
  drag_dest_->set_delegate(delegate);
}

void TabContentsViewGtk::InsertIntoContentArea(GtkWidget* widget) {
  gtk_container_add(GTK_CONTAINER(expanded_.get()), widget);
}

// Called when the content view gtk widget is tabbed to, or after the call to
// gtk_widget_child_focus() in TakeFocus(). We return true
// and grab focus if we don't have it. The call to
// FocusThroughTabTraversal(bool) forwards the "move focus forward" effect to
// webkit.
gboolean TabContentsViewGtk::OnFocus(GtkWidget* widget,
                                     GtkDirectionType focus) {
  // Give our view wrapper first chance at this event.
  if (view_wrapper_.get()) {
    gboolean return_value = FALSE;
    if (view_wrapper_->OnNativeViewFocusEvent(widget, focus, &return_value))
      return return_value;
  }

  // If we already have focus, let the next widget have a shot at it. We will
  // reach this situation after the call to gtk_widget_child_focus() in
  // TakeFocus().
  if (gtk_widget_is_focus(widget))
    return FALSE;

  gtk_widget_grab_focus(widget);
  bool reverse = focus == GTK_DIR_TAB_BACKWARD;
  tab_contents_->FocusThroughTabTraversal(reverse);
  return TRUE;
}

void TabContentsViewGtk::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  delegate_view_helper_.CreateNewWindowFromTabContents(
      tab_contents_, route_id, params);
}

void TabContentsViewGtk::CreateNewWidget(
    int route_id, WebKit::WebPopupType popup_type) {
  delegate_view_helper_.CreateNewWidget(route_id, popup_type,
      tab_contents_->render_view_host()->process());
}

void TabContentsViewGtk::CreateNewFullscreenWidget(int route_id) {
  delegate_view_helper_.CreateNewFullscreenWidget(
      route_id, tab_contents_->render_view_host()->process());
}

void TabContentsViewGtk::ShowCreatedWindow(int route_id,
                                           WindowOpenDisposition disposition,
                                           const gfx::Rect& initial_pos,
                                           bool user_gesture) {
  delegate_view_helper_.ShowCreatedWindow(
      tab_contents_, route_id, disposition, initial_pos, user_gesture);
}

void TabContentsViewGtk::ShowCreatedWidget(
    int route_id, const gfx::Rect& initial_pos) {
  delegate_view_helper_.ShowCreatedWidget(
      tab_contents_, route_id, initial_pos);
}

void TabContentsViewGtk::ShowCreatedFullscreenWidget(int route_id) {
  delegate_view_helper_.ShowCreatedFullscreenWidget(tab_contents_, route_id);
}

void TabContentsViewGtk::ShowContextMenu(const ContextMenuParams& params) {
  if (wrapper())
    wrapper()->ShowContextMenu(params);
  else
    DLOG(ERROR) << "Implement context menus without chrome/ code";
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

  RenderWidgetHostViewGtk* view_gtk = static_cast<RenderWidgetHostViewGtk*>(
      tab_contents_->GetRenderWidgetHostView());
  if (!view_gtk || !view_gtk->last_mouse_down())
    return;

  drag_source_->StartDragging(drop_data, ops, view_gtk->last_mouse_down(),
                              image, image_offset);
}

// -----------------------------------------------------------------------------

void TabContentsViewGtk::OnChildSizeRequest(GtkWidget* widget,
                                            GtkWidget* child,
                                            GtkRequisition* requisition) {
  if (tab_contents_->delegate()) {
    requisition->height +=
        tab_contents_->delegate()->GetExtraRenderViewHeight();
  }
}

void TabContentsViewGtk::OnSizeAllocate(GtkWidget* widget,
                                        GtkAllocation* allocation) {
  int width = allocation->width;
  int height = allocation->height;
  // |delegate()| can be NULL here during browser teardown.
  if (tab_contents_->delegate())
    height += tab_contents_->delegate()->GetExtraRenderViewHeight();
  gfx::Size size(width, height);
  requested_size_ = size;

  // We manually tell our RWHV to resize the renderer content.  This avoids
  // spurious resizes from GTK+.
  RenderWidgetHostView* rwhv = tab_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
  if (tab_contents_->interstitial_page())
    tab_contents_->interstitial_page()->SetSize(size);
}
