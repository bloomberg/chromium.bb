// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/tab_contents_view_gtk.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_factory.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/web_drag_dest_gtk.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents_drag_source.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "chrome/browser/ui/views/tab_contents/render_view_context_menu_views.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "views/controls/native/native_view_host.h"
#include "views/focus/view_storage.h"
#include "views/screen.h"
#include "views/widget/root_view.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;
using WebKit::WebInputEvent;


namespace {

// Called when the content view gtk widget is tabbed to, or after the call to
// gtk_widget_child_focus() in TakeFocus(). We return true
// and grab focus if we don't have it. The call to
// FocusThroughTabTraversal(bool) forwards the "move focus forward" effect to
// webkit.
gboolean OnFocus(GtkWidget* widget, GtkDirectionType focus,
                 TabContents* tab_contents) {
  // If we already have focus, let the next widget have a shot at it. We will
  // reach this situation after the call to gtk_widget_child_focus() in
  // TakeFocus().
  if (gtk_widget_is_focus(widget))
    return FALSE;

  gtk_widget_grab_focus(widget);
  bool reverse = focus == GTK_DIR_TAB_BACKWARD;
  tab_contents->FocusThroughTabTraversal(reverse);
  return TRUE;
}

// Called when the mouse leaves the widget. We notify our delegate.
// WidgetGtk also defines OnLeaveNotify, so we use the name OnLeaveNotify2
// here.
gboolean OnLeaveNotify2(GtkWidget* widget, GdkEventCrossing* event,
                        TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(
        tab_contents, views::Screen::GetCursorScreenPoint(), false);
  return FALSE;
}

// Called when the mouse moves within the widget.
gboolean CallMouseMove(GtkWidget* widget, GdkEventMotion* event,
                       TabContentsViewGtk* tab_contents_view) {
  return tab_contents_view->OnMouseMove(widget, event);
}

// See tab_contents_view_gtk.cc for discussion of mouse scroll zooming.
gboolean OnMouseScroll(GtkWidget* widget, GdkEventScroll* event,
                       TabContents* tab_contents) {
  if ((event->state & gtk_accelerator_get_default_mod_mask()) ==
      GDK_CONTROL_MASK) {
    if (tab_contents->delegate()) {
      if (event->direction == GDK_SCROLL_DOWN) {
        tab_contents->delegate()->ContentsZoomChange(false);
        return TRUE;
      } else if (event->direction == GDK_SCROLL_UP) {
        tab_contents->delegate()->ContentsZoomChange(true);
        return TRUE;
      }
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
      views::WidgetGtk(TYPE_CHILD),
      sad_tab_(NULL),
      ignore_next_char_event_(false) {
  drag_source_.reset(new TabContentsDragSource(this));
  last_focused_view_storage_id_ =
      views::ViewStorage::GetInstance()->CreateStorageID();
}

TabContentsViewGtk::~TabContentsViewGtk() {
  // Make sure to remove any stored view we may still have in the ViewStorage.
  //
  // It is possible the view went away before us, so we only do this if the
  // view is registered.
  views::ViewStorage* view_storage = views::ViewStorage::GetInstance();
  if (view_storage->RetrieveView(last_focused_view_storage_id_) != NULL)
    view_storage->RemoveView(last_focused_view_storage_id_);

  // Just deleting the object doesn't destroy the GtkWidget. We need to do that
  // manually, and synchronously, since subsequent signal handlers may expect
  // to locate this object.
  CloseNow();
}

void TabContentsViewGtk::AttachConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  DCHECK(find(constrained_windows_.begin(), constrained_windows_.end(),
              constrained_window) == constrained_windows_.end());

  constrained_windows_.push_back(constrained_window);
  AddChild(constrained_window->widget());

  gfx::Rect bounds;
  GetContainerBounds(&bounds);
  SetFloatingPosition(bounds.size());
}

void TabContentsViewGtk::RemoveConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  std::vector<ConstrainedWindowGtk*>::iterator item =
      find(constrained_windows_.begin(), constrained_windows_.end(),
           constrained_window);
  DCHECK(item != constrained_windows_.end());
  RemoveChild((*item)->widget());
  constrained_windows_.erase(item);
}

void TabContentsViewGtk::CreateView(const gfx::Size& initial_size) {
  set_delete_on_destroy(false);
  WidgetGtk::Init(NULL, gfx::Rect(0, 0, initial_size.width(),
                                  initial_size.height()));
  // We need to own the widget in order to attach/detach the native view
  // to container.
  gtk_object_ref(GTK_OBJECT(GetNativeView()));
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

  // If we were showing sad tab, remove it now.
  if (sad_tab_ != NULL) {
    SetContentsView(new views::View());
    sad_tab_ = NULL;
  }

  RenderWidgetHostViewGtk* view =
      new RenderWidgetHostViewGtk(render_widget_host);
  view->InitAsChild();
  g_signal_connect(view->native_view(), "focus",
                   G_CALLBACK(OnFocus), tab_contents());
  g_signal_connect(view->native_view(), "leave-notify-event",
                   G_CALLBACK(OnLeaveNotify2), tab_contents());
  g_signal_connect(view->native_view(), "motion-notify-event",
                   G_CALLBACK(CallMouseMove), this);
  g_signal_connect(view->native_view(), "scroll-event",
                   G_CALLBACK(OnMouseScroll), tab_contents());
  gtk_widget_add_events(view->native_view(), GDK_LEAVE_NOTIFY_MASK |
                        GDK_POINTER_MOTION_MASK);

  // Renderer target DnD.
  if (tab_contents()->ShouldAcceptDragAndDrop())
    drag_dest_.reset(new WebDragDestGtk(tab_contents(), view->native_view()));

  gtk_fixed_put(GTK_FIXED(GetNativeView()), view->native_view(), 0, 0);
  return view;
}

gfx::NativeView TabContentsViewGtk::GetNativeView() const {
  return WidgetGtk::GetNativeView();
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
  // Callers expect the requested bounds not the actual bounds. For example,
  // during init callers expect 0x0, but Gtk layout enforces a min size of 1x1.
  GetBounds(out, false);

  gfx::Size size;
  WidgetGtk::GetRequestedSize(&size);
  out->set_size(size);
}

void TabContentsViewGtk::StartDragging(const WebDropData& drop_data,
                                       WebDragOperationsMask ops,
                                       const SkBitmap& image,
                                       const gfx::Point& image_offset) {
  drag_source_->StartDragging(drop_data, ops, &last_mouse_down_,
                              image, image_offset);
}

void TabContentsViewGtk::SetPageTitle(const std::wstring& title) {
  // Set the window name to include the page title so it's easier to spot
  // when debugging (e.g. via xwininfo -tree).
  gfx::NativeView content_view = GetContentNativeView();
  if (content_view && content_view->window)
    gdk_window_set_title(content_view->window, WideToUTF8(title).c_str());
}

void TabContentsViewGtk::OnTabCrashed(base::TerminationStatus /* status */,
                                      int /* error_code */) {
}

void TabContentsViewGtk::SizeContents(const gfx::Size& size) {
  // TODO(brettw) this is a hack and should be removed. See tab_contents_view.h.

  // We're contained in a fixed. To have the fixed relay us out to |size|, set
  // the size request, which triggers OnSizeAllocate.
  gtk_widget_set_size_request(GetNativeView(), size.width(), size.height());

  // We need to send this immediately.
  RenderWidgetHostView* rwhv = tab_contents()->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
}

void TabContentsViewGtk::Focus() {
  if (tab_contents()->interstitial_page()) {
    tab_contents()->interstitial_page()->Focus();
    return;
  }

  if (tab_contents()->is_crashed() && sad_tab_ != NULL) {
    sad_tab_->RequestFocus();
    return;
  }

  RenderWidgetHostView* rwhv = tab_contents()->GetRenderWidgetHostView();
  gtk_widget_grab_focus(rwhv ? rwhv->GetNativeView() : GetNativeView());
}

void TabContentsViewGtk::SetInitialFocus() {
  if (tab_contents()->FocusLocationBarByDefault())
    tab_contents()->SetFocusToLocationBar(false);
  else
    Focus();
}

void TabContentsViewGtk::StoreFocus() {
  views::ViewStorage* view_storage = views::ViewStorage::GetInstance();

  if (view_storage->RetrieveView(last_focused_view_storage_id_) != NULL)
    view_storage->RemoveView(last_focused_view_storage_id_);

  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManagerForNativeView(GetNativeView());
  if (focus_manager) {
    // |focus_manager| can be NULL if the tab has been detached but still
    // exists.
    views::View* focused_view = focus_manager->GetFocusedView();
    if (focused_view)
      view_storage->StoreView(last_focused_view_storage_id_, focused_view);
  }
}

void TabContentsViewGtk::RestoreFocus() {
  views::ViewStorage* view_storage = views::ViewStorage::GetInstance();
  views::View* last_focused_view =
      view_storage->RetrieveView(last_focused_view_storage_id_);
  if (!last_focused_view) {
    SetInitialFocus();
  } else {
    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManagerForNativeView(GetNativeView());

    // If you hit this DCHECK, please report it to Jay (jcampan).
    DCHECK(focus_manager != NULL) << "No focus manager when restoring focus.";

    if (last_focused_view->IsFocusableInRootView() && focus_manager &&
        focus_manager->ContainsView(last_focused_view)) {
      last_focused_view->RequestFocus();
    } else {
      // The focused view may not belong to the same window hierarchy (e.g.
      // if the location bar was focused and the tab is dragged out), or it may
      // no longer be focusable (e.g. if the location bar was focused and then
      // we switched to fullscreen mode).  In that case we default to the
      // default focus.
      SetInitialFocus();
    }
    view_storage->RemoveView(last_focused_view_storage_id_);
  }
}

void TabContentsViewGtk::GetViewBounds(gfx::Rect* out) const {
  GetBounds(out, true);
}

void TabContentsViewGtk::UpdateDragCursor(WebDragOperation operation) {
  if (drag_dest_.get())
    drag_dest_->UpdateDragStatus(operation);
}

void TabContentsViewGtk::GotFocus() {
  if (tab_contents()->delegate())
    tab_contents()->delegate()->TabContentsFocused(tab_contents());
}

void TabContentsViewGtk::TakeFocus(bool reverse) {
  if (tab_contents()->delegate() &&
      !tab_contents()->delegate()->TakeFocus(reverse)) {

    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManagerForNativeView(GetNativeView());

    // We may not have a focus manager if the tab has been switched before this
    // message arrived.
    if (focus_manager)
      focus_manager->AdvanceFocus(reverse);
  }
}

void TabContentsViewGtk::ShowContextMenu(const ContextMenuParams& params) {
  // Allow delegates to handle the context menu operation first.
  if (tab_contents()->delegate()->HandleContextMenu(params))
    return;

  context_menu_.reset(new RenderViewContextMenuViews(tab_contents(), params));
  context_menu_->Init();

  gfx::Point screen_point(params.x, params.y);
  views::View::ConvertPointToScreen(GetRootView(), &screen_point);

  // Enable recursive tasks on the message loop so we can get updates while
  // the context menu is being displayed.
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  context_menu_->RunMenuAt(screen_point.x(), screen_point.y());
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void TabContentsViewGtk::ShowPopupMenu(const gfx::Rect& bounds,
                                       int item_height,
                                       double item_font_size,
                                       int selected_item,
                                       const std::vector<WebMenuItem>& items,
                                       bool right_aligned) {
  // External popup menus are only used on Mac.
  NOTREACHED();
}

gboolean TabContentsViewGtk::OnButtonPress(GtkWidget* widget,
                                           GdkEventButton* event) {
  last_mouse_down_ = *event;
  return views::WidgetGtk::OnButtonPress(widget, event);
}

void TabContentsViewGtk::OnSizeAllocate(GtkWidget* widget,
                                        GtkAllocation* allocation) {
  gfx::Size new_size(allocation->width, allocation->height);

  // Always call WasSized() to allow checking to make sure the
  // RenderWidgetHostView is the right size.
  WasSized(new_size);
}

gboolean TabContentsViewGtk::OnPaint(GtkWidget* widget, GdkEventExpose* event) {
  if (tab_contents()->render_view_host() &&
      !tab_contents()->render_view_host()->IsRenderViewLive()) {
    base::TerminationStatus status =
        tab_contents()->render_view_host()->render_view_termination_status();
    SadTabView::Kind kind =
        status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ?
        SadTabView::KILLED : SadTabView::CRASHED;
    sad_tab_ = new SadTabView(tab_contents(), kind);
    SetContentsView(sad_tab_);
    gfx::Rect bounds;
    GetBounds(&bounds, true);
    sad_tab_->SetBoundsRect(gfx::Rect(0, 0, bounds.width(), bounds.height()));
    gfx::CanvasSkiaPaint canvas(event);
    sad_tab_->ProcessPaint(&canvas);
  }
  return false;  // False indicates other widgets should get the event as well.
}

void TabContentsViewGtk::OnShow(GtkWidget* widget) {
  WasShown();
}

void TabContentsViewGtk::OnHide(GtkWidget* widget) {
  WasHidden();
}

void TabContentsViewGtk::WasHidden() {
  tab_contents()->HideContents();
}

void TabContentsViewGtk::WasShown() {
  tab_contents()->ShowContents();
}

void TabContentsViewGtk::WasSized(const gfx::Size& size) {
  // We have to check that the RenderWidgetHostView is the proper size.
  // It can be wrong in cases where the renderer has died and the host
  // view needed to be recreated.
  bool needs_resize = size != size_;

  if (needs_resize) {
    size_ = size;
    if (tab_contents()->interstitial_page())
      tab_contents()->interstitial_page()->SetSize(size);
  }

  RenderWidgetHostView* rwhv = tab_contents()->GetRenderWidgetHostView();
  if (rwhv && rwhv->GetViewBounds().size() != size)
    rwhv->SetSize(size);

  if (needs_resize)
    SetFloatingPosition(size);
}

void TabContentsViewGtk::SetFloatingPosition(const gfx::Size& size) {
  // Place each ConstrainedWindow in the center of the view.
  int half_view_width = size.width() / 2;

  typedef std::vector<ConstrainedWindowGtk*>::iterator iterator;

  for (iterator f = constrained_windows_.begin(),
                l = constrained_windows_.end(); f != l; ++f) {
    GtkWidget* widget = (*f)->widget();

    GtkRequisition requisition;
    gtk_widget_size_request(widget, &requisition);

    int child_x = std::max(half_view_width - (requisition.width / 2), 0);
    PositionChild(widget, child_x, 0, 0, 0);
  }
}

// Called when the mouse moves within the widget. We notify SadTabView if it's
// not NULL, else our delegate.
gboolean TabContentsViewGtk::OnMouseMove(GtkWidget* widget,
                                         GdkEventMotion* event) {
  if (sad_tab_ != NULL)
    WidgetGtk::OnMotionNotify(widget, event);
  else if (tab_contents()->delegate())
    tab_contents()->delegate()->ContentsMouseEvent(
        tab_contents(), views::Screen::GetCursorScreenPoint(), true);
  return FALSE;
}
