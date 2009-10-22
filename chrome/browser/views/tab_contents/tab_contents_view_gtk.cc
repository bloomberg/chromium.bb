// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/tab_contents/tab_contents_view_gtk.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "app/gfx/canvas_paint.h"
#include "base/string_util.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/gtk/constrained_window_gtk.h"
#include "chrome/browser/gtk/tab_contents_drag_source.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_factory.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/web_drag_dest_gtk.h"
#include "chrome/browser/views/blocked_popup_container_view_views.h"
#include "chrome/browser/views/sad_tab_view.h"
#include "chrome/browser/views/tab_contents/render_view_context_menu_win.h"
#include "views/focus/view_storage.h"
#include "views/controls/native/native_view_host.h"
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
    tab_contents->delegate()->ContentsMouseEvent(tab_contents, false);
  return FALSE;
}

// Called when the mouse moves within the widget. We notify our delegate.
gboolean OnMouseMove(GtkWidget* widget, GdkEventMotion* event,
                     TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(tab_contents, true);
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
      views::WidgetGtk(TYPE_CHILD),
      ignore_next_char_event_(false) {
  drag_source_.reset(new TabContentsDragSource(this));
  last_focused_view_storage_id_ =
      views::ViewStorage::GetSharedInstance()->CreateStorageID();
}

TabContentsViewGtk::~TabContentsViewGtk() {
  // Make sure to remove any stored view we may still have in the ViewStorage.
  //
  // It is possible the view went away before us, so we only do this if the
  // view is registered.
  views::ViewStorage* view_storage = views::ViewStorage::GetSharedInstance();
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
  g_signal_connect(view->native_view(), "focus",
                   G_CALLBACK(OnFocus), tab_contents());
  g_signal_connect(view->native_view(), "leave-notify-event",
                   G_CALLBACK(OnLeaveNotify2), tab_contents());
  g_signal_connect(view->native_view(), "motion-notify-event",
                   G_CALLBACK(OnMouseMove), tab_contents());
  g_signal_connect(view->native_view(), "scroll-event",
                   G_CALLBACK(OnMouseScroll), tab_contents());
  gtk_widget_add_events(view->native_view(), GDK_LEAVE_NOTIFY_MASK |
                        GDK_POINTER_MOTION_MASK);

  // Renderer target DnD.
  drag_dest_.reset(new WebDragDestGtk(tab_contents(), view->native_view()));

  gtk_fixed_put(GTK_FIXED(GetNativeView()), view->native_view(), 0, 0);
  return view;
}

gfx::NativeView TabContentsViewGtk::GetNativeView() const {
  return WidgetGtk::GetNativeView();
}

gfx::NativeView TabContentsViewGtk::GetContentNativeView() const {
  if (!tab_contents()->render_widget_host_view())
    return NULL;
  return tab_contents()->render_widget_host_view()->GetNativeView();
}

gfx::NativeWindow TabContentsViewGtk::GetTopLevelNativeWindow() const {
  GtkWidget* window = gtk_widget_get_ancestor(GetNativeView(), GTK_TYPE_WINDOW);
  return window ? GTK_WINDOW(window) : NULL;
}

void TabContentsViewGtk::GetContainerBounds(gfx::Rect* out) const {
  GetBounds(out, false);
}

void TabContentsViewGtk::StartDragging(const WebDropData& drop_data,
                                       WebDragOperationsMask ops) {
  drag_source_->StartDragging(drop_data, &last_mouse_down_);
  // TODO(snej): Make use of WebDragOperationsMask
}

void TabContentsViewGtk::SetPageTitle(const std::wstring& title) {
  // Set the window name to include the page title so it's easier to spot
  // when debugging (e.g. via xwininfo -tree).
  gfx::NativeView content_view = GetContentNativeView();
  if (content_view && content_view->window)
    gdk_window_set_title(content_view->window, WideToUTF8(title).c_str());
}

void TabContentsViewGtk::OnTabCrashed() {
}

void TabContentsViewGtk::SizeContents(const gfx::Size& size) {
  // TODO(brettw) this is a hack and should be removed. See tab_contents_view.h.
  WasSized(size);
}

void TabContentsViewGtk::Focus() {
  if (tab_contents()->interstitial_page()) {
    tab_contents()->interstitial_page()->Focus();
    return;
  }

  if (sad_tab_.get()) {
    sad_tab_->RequestFocus();
    return;
  }

  RenderWidgetHostView* rwhv = tab_contents()->render_widget_host_view();
  gtk_widget_grab_focus(rwhv ? rwhv->GetNativeView() : GetNativeView());
}

void TabContentsViewGtk::SetInitialFocus() {
  if (tab_contents()->FocusLocationBarByDefault())
    tab_contents()->delegate()->SetFocusToLocationBar();
  else
    Focus();
}

void TabContentsViewGtk::StoreFocus() {
  views::ViewStorage* view_storage = views::ViewStorage::GetSharedInstance();

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
  views::ViewStorage* view_storage = views::ViewStorage::GetSharedInstance();
  views::View* last_focused_view =
      view_storage->RetrieveView(last_focused_view_storage_id_);
  if (!last_focused_view) {
    SetInitialFocus();
  } else {
    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManagerForNativeView(GetNativeView());

    // If you hit this DCHECK, please report it to Jay (jcampan).
    DCHECK(focus_manager != NULL) << "No focus manager when restoring focus.";

    if (last_focused_view->IsFocusable() && focus_manager &&
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

void TabContentsViewGtk::UpdateDragCursor(WebDragOperation operation) {
  drag_dest_->UpdateDragStatus(operation);
}

void TabContentsViewGtk::GotFocus() {
  tab_contents()->delegate()->TabContentsFocused(tab_contents());
}

void TabContentsViewGtk::TakeFocus(bool reverse) {
  // This is called when we the renderer asks us to take focus back (i.e., it
  // has iterated past the last focusable element on the page).
  gtk_widget_child_focus(GTK_WIDGET(GetTopLevelNativeWindow()),
      reverse ? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD);
}

void TabContentsViewGtk::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  // The renderer returned a keyboard event it did not process. This may be
  // a keyboard shortcut that we have to process.
  if (event.type != WebInputEvent::RawKeyDown)
    return;

  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManagerForNativeView(GetNativeView());
  // We may not have a focus_manager at this point (if the tab has been switched
  // by the time this message returned).
  if (!focus_manager)
    return;

  bool shift_pressed = (event.modifiers & WebInputEvent::ShiftKey) ==
                       WebInputEvent::ShiftKey;
  bool ctrl_pressed = (event.modifiers & WebInputEvent::ControlKey) ==
                      WebInputEvent::ControlKey;
  bool alt_pressed = (event.modifiers & WebInputEvent::AltKey) ==
                     WebInputEvent::AltKey;

  focus_manager->ProcessAccelerator(
      views::Accelerator(static_cast<base::KeyboardCode>(event.windowsKeyCode),
                         shift_pressed, ctrl_pressed, alt_pressed));
  // DANGER: |this| could be deleted now!

  // Note that we do not handle Gtk mnemonics/accelerators or binding set here
  // (as it is done in BrowserWindowGtk::HandleKeyboardEvent), as we override
  // Gtk behavior completely.
}

void TabContentsViewGtk::ShowContextMenu(const ContextMenuParams& params) {
  // Allow delegates to handle the context menu operation first.
  if (tab_contents()->delegate()->HandleContextMenu(params))
    return;

  context_menu_.reset(new RenderViewContextMenuWin(tab_contents(), params));
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

gboolean TabContentsViewGtk::OnButtonPress(GtkWidget* widget,
                                           GdkEventButton* event) {
  last_mouse_down_ = *event;
  return views::WidgetGtk::OnButtonPress(widget, event);
}

void TabContentsViewGtk::OnSizeAllocate(GtkWidget* widget,
                                        GtkAllocation* allocation) {
  gfx::Size new_size(allocation->width, allocation->height);
  if (new_size == size_)
    return;

  WasSized(new_size);
}

void TabContentsViewGtk::OnPaint(GtkWidget* widget, GdkEventExpose* event) {
  if (tab_contents()->render_view_host() &&
      !tab_contents()->render_view_host()->IsRenderViewLive()) {
    if (!sad_tab_.get())
      sad_tab_.reset(new SadTabView);
    gfx::Rect bounds;
    GetBounds(&bounds, true);
    sad_tab_->SetBounds(gfx::Rect(0, 0, bounds.width(), bounds.height()));
    gfx::CanvasPaint canvas(event);
    sad_tab_->ProcessPaint(&canvas);
  }
}

void TabContentsViewGtk::WasHidden() {
  tab_contents()->HideContents();
}

void TabContentsViewGtk::WasShown() {
  tab_contents()->ShowContents();
}

void TabContentsViewGtk::WasSized(const gfx::Size& size) {
  size_ = size;
  if (tab_contents()->interstitial_page())
    tab_contents()->interstitial_page()->SetSize(size);
  if (tab_contents()->render_widget_host_view())
    tab_contents()->render_widget_host_view()->SetSize(size);

  // TODO(brettw) this function can probably be moved to this class.
  tab_contents()->RepositionSupressedPopupsToFit();

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
    PositionChild(widget, child_x, 0, requisition.width, requisition.height);
  }
}

