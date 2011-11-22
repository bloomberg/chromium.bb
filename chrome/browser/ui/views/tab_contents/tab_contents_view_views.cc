// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/tab_contents_view_views.h"

#include <vector>

#include "base/time.h"
#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view.h"
#include "chrome/browser/ui/views/tab_contents/render_view_context_menu_views.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/public/browser/render_process_host.h"
#include "ui/gfx/screen.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

using WebKit::WebDragOperation;
using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationsMask;
using WebKit::WebInputEvent;

TabContentsViewViews::TabContentsViewViews(TabContents* tab_contents)
    : tab_contents_(tab_contents),
      native_tab_contents_view_(NULL),
      close_tab_after_drag_ends_(false),
      focus_manager_(NULL),
      overlaid_view_(NULL) {
  last_focused_view_storage_id_ =
      views::ViewStorage::GetInstance()->CreateStorageID();
}

TabContentsViewViews::~TabContentsViewViews() {
  // Makes sure to remove any stored view we may still have in the ViewStorage.
  //
  // It is possible the view went away before us, so we only do this if the
  // view is registered.
  views::ViewStorage* view_storage = views::ViewStorage::GetInstance();
  if (view_storage->RetrieveView(last_focused_view_storage_id_) != NULL)
    view_storage->RemoveView(last_focused_view_storage_id_);
}

void TabContentsViewViews::AttachConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
}
void TabContentsViewViews::RemoveConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
}

void TabContentsViewViews::Unparent() {
  // Remember who our FocusManager is, we won't be able to access it once
  // un-parented.
  focus_manager_ = Widget::GetFocusManager();
  CHECK(native_tab_contents_view_);
  native_tab_contents_view_->Unparent();
}

void TabContentsViewViews::CreateView(const gfx::Size& initial_size) {
  native_tab_contents_view_ =
      NativeTabContentsView::CreateNativeTabContentsView(this);
  native_tab_contents_view_->InitNativeTabContentsView();
}

RenderWidgetHostView* TabContentsViewViews::CreateViewForWidget(
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

  return native_tab_contents_view_->CreateRenderWidgetHostView(
      render_widget_host);
}

gfx::NativeView TabContentsViewViews::GetNativeView() const {
  return Widget::GetNativeView();
}

gfx::NativeView TabContentsViewViews::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = tab_contents_->GetRenderWidgetHostView();
  return rwhv ? rwhv->GetNativeView() : NULL;
}

gfx::NativeWindow TabContentsViewViews::GetTopLevelNativeWindow() const {
  const Widget* toplevel = GetTopLevelWidget();
  return toplevel ? toplevel->GetNativeWindow() : NULL;
}

void TabContentsViewViews::GetContainerBounds(gfx::Rect* out) const {
  *out = GetClientAreaScreenBounds();
}

void TabContentsViewViews::StartDragging(const WebDropData& drop_data,
                                         WebDragOperationsMask ops,
                                         const SkBitmap& image,
                                         const gfx::Point& image_offset) {
  native_tab_contents_view_->StartDragging(drop_data, ops, image, image_offset);
}

void TabContentsViewViews::SetPageTitle(const string16& title) {
  native_tab_contents_view_->SetPageTitle(title);
}

void TabContentsViewViews::OnTabCrashed(base::TerminationStatus status,
                                        int /* error_code */) {
}

void TabContentsViewViews::SizeContents(const gfx::Size& size) {
  gfx::Rect bounds;
  GetContainerBounds(&bounds);
  if (bounds.size() != size) {
    SetSize(size);
  } else {
    // Our size matches what we want but the renderers size may not match.
    // Pretend we were resized so that the renderers size is updated too.
    OnNativeTabContentsViewSized(size);
  }
}

void TabContentsViewViews::RenderViewCreated(RenderViewHost* host) {
}

void TabContentsViewViews::Focus() {
  if (tab_contents_->interstitial_page()) {
    tab_contents_->interstitial_page()->Focus();
    return;
  }

  if (overlaid_view_) {
    overlaid_view_->GetContentsView()->RequestFocus();
    return;
  }

  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents_);
  if (wrapper) {
    // TODO(erg): TabContents used to own constrained windows, which is why
    // this is here. Eventually this should be ported to a containing view
    // specializing in constrained window management.
    ConstrainedWindowTabHelper* helper =
        wrapper->constrained_window_tab_helper();
    if (helper->constrained_window_count() > 0) {
      ConstrainedWindow* window = *helper->constrained_window_begin();
      DCHECK(window);
      window->FocusConstrainedWindow();
      return;
    }
  }

  // Give focus to this tab content view.
  RenderWidgetHostView* rwhv = tab_contents_->GetRenderWidgetHostView();
  if (rwhv) {
    rwhv->Focus();
    // rwhvv may not have a focus manager, so we try with our focus manager
    // also if rwhvv cannot acquire focus.
    if (rwhv->HasFocus())
      return;
  }
  views::FocusManager* focus_manager = GetFocusManager();
  if (focus_manager) {
    // if rwhvv cannot focus, or we don't have a rwhv, we try focusing
    // on the current tab content view.
    focus_manager->SetFocusedView(GetContentsView());
  }
}

void TabContentsViewViews::SetInitialFocus() {
  if (tab_contents_->FocusLocationBarByDefault())
    tab_contents_->SetFocusToLocationBar(false);
  else
    Focus();
}

void TabContentsViewViews::StoreFocus() {
  views::ViewStorage* view_storage = views::ViewStorage::GetInstance();

  if (view_storage->RetrieveView(last_focused_view_storage_id_) != NULL)
    view_storage->RemoveView(last_focused_view_storage_id_);

  views::FocusManager* focus_manager = GetFocusManager();
  if (focus_manager) {
    // |focus_manager| can be NULL if the tab has been detached but still
    // exists.
    views::View* focused_view = focus_manager->GetFocusedView();
    if (focused_view)
      view_storage->StoreView(last_focused_view_storage_id_, focused_view);
  }
}

void TabContentsViewViews::RestoreFocus() {
  views::ViewStorage* view_storage = views::ViewStorage::GetInstance();
  views::View* last_focused_view =
      view_storage->RetrieveView(last_focused_view_storage_id_);

  if (!last_focused_view) {
    SetInitialFocus();
  } else {
    views::FocusManager* focus_manager = GetFocusManager();
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

bool TabContentsViewViews::IsDoingDrag() const {
  return native_tab_contents_view_->IsDoingDrag();
}

void TabContentsViewViews::CancelDragAndCloseTab() {
  DCHECK(IsDoingDrag());
  // We can't close the tab while we're in the drag and
  // |drag_handler_->CancelDrag()| is async.  Instead, set a flag to cancel
  // the drag and when the drag nested message loop ends, close the tab.
  native_tab_contents_view_->CancelDrag();
  close_tab_after_drag_ends_ = true;
}

bool TabContentsViewViews::IsEventTracking() const {
  return false;
}

void TabContentsViewViews::CloseTabAfterEventTracking() {
}

void TabContentsViewViews::GetViewBounds(gfx::Rect* out) const {
  *out = GetWindowScreenBounds();
}

void TabContentsViewViews::InstallOverlayView(gfx::NativeView view) {
  DCHECK(!overlaid_view_);
  views::Widget::ReparentNativeView(view, GetNativeView());
  overlaid_view_ = views::Widget::GetWidgetForNativeView(view);
  overlaid_view_->SetBounds(gfx::Rect(GetClientAreaScreenBounds().size()));
}

void TabContentsViewViews::RemoveOverlayView() {
  DCHECK(overlaid_view_);
  overlaid_view_->Close();
  overlaid_view_ = NULL;
}

void TabContentsViewViews::UpdateDragCursor(WebDragOperation operation) {
  native_tab_contents_view_->SetDragCursor(operation);
}

void TabContentsViewViews::GotFocus() {
  if (tab_contents_->delegate())
    tab_contents_->delegate()->TabContentsFocused(tab_contents_);
}

void TabContentsViewViews::TakeFocus(bool reverse) {
  if (tab_contents_->delegate() &&
      !tab_contents_->delegate()->TakeFocus(reverse)) {
    views::FocusManager* focus_manager = GetFocusManager();

    // We may not have a focus manager if the tab has been switched before this
    // message arrived.
    if (focus_manager)
      focus_manager->AdvanceFocus(reverse);
  }
}

void TabContentsViewViews::CloseTab() {
  tab_contents_->Close(tab_contents_->render_view_host());
}

void TabContentsViewViews::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  delegate_view_helper_.CreateNewWindowFromTabContents(
      tab_contents_, route_id, params);
}

void TabContentsViewViews::CreateNewWidget(
    int route_id, WebKit::WebPopupType popup_type) {
  delegate_view_helper_.CreateNewWidget(route_id, popup_type,
      tab_contents_->render_view_host()->process());
}

void TabContentsViewViews::CreateNewFullscreenWidget(int route_id) {
  delegate_view_helper_.CreateNewFullscreenWidget(
      route_id, tab_contents_->render_view_host()->process());
}

void TabContentsViewViews::ShowCreatedWindow(int route_id,
                                             WindowOpenDisposition disposition,
                                             const gfx::Rect& initial_pos,
                                             bool user_gesture) {
  delegate_view_helper_.ShowCreatedWindow(
      tab_contents_, route_id, disposition, initial_pos, user_gesture);
}

void TabContentsViewViews::ShowCreatedWidget(
    int route_id, const gfx::Rect& initial_pos) {
  delegate_view_helper_.ShowCreatedWidget(
      tab_contents_, route_id, initial_pos);
}

void TabContentsViewViews::ShowCreatedFullscreenWidget(int route_id) {
  delegate_view_helper_.ShowCreatedFullscreenWidget(tab_contents_, route_id);
}

void TabContentsViewViews::ShowContextMenu(const ContextMenuParams& params) {
  // Allow delegates to handle the context menu operation first.
  if (tab_contents_->delegate() &&
      tab_contents_->delegate()->HandleContextMenu(params)) {
    return;
  }

  context_menu_.reset(new RenderViewContextMenuViews(tab_contents_, params));
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

void TabContentsViewViews::ShowPopupMenu(const gfx::Rect& bounds,
                                         int item_height,
                                         double item_font_size,
                                         int selected_item,
                                         const std::vector<WebMenuItem>& items,
                                         bool right_aligned) {
  // External popup menus are only used on Mac.
  NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsViewViews, internal::NativeTabContentsViewDelegate implementation:

TabContents* TabContentsViewViews::GetTabContents() {
  return tab_contents_;
}

bool TabContentsViewViews::IsShowingSadTab() const {
  return tab_contents_->is_crashed() && overlaid_view_;
}

void TabContentsViewViews::OnNativeTabContentsViewShown() {
  tab_contents_->ShowContents();
}

void TabContentsViewViews::OnNativeTabContentsViewHidden() {
  tab_contents_->HideContents();
}

void TabContentsViewViews::OnNativeTabContentsViewSized(const gfx::Size& size) {
  if (tab_contents_->interstitial_page())
    tab_contents_->interstitial_page()->SetSize(size);
  RenderWidgetHostView* rwhv = tab_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
}

void TabContentsViewViews::OnNativeTabContentsViewWheelZoom(bool zoom_in) {
  if (tab_contents_->delegate())
    tab_contents_->delegate()->ContentsZoomChange(zoom_in);
}

void TabContentsViewViews::OnNativeTabContentsViewMouseDown() {
  // Make sure this TabContents is activated when it is clicked on.
  if (tab_contents_->delegate())
    tab_contents_->delegate()->ActivateContents(tab_contents_);
}

void TabContentsViewViews::OnNativeTabContentsViewMouseMove(bool motion) {
  // Let our delegate know that the mouse moved (useful for resetting status
  // bubble state).
  if (tab_contents_->delegate()) {
    tab_contents_->delegate()->ContentsMouseEvent(
        tab_contents_, gfx::Screen::GetCursorScreenPoint(), motion);
  }
}

void TabContentsViewViews::OnNativeTabContentsViewDraggingEnded() {
  if (close_tab_after_drag_ends_) {
    close_tab_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(0),
                           this, &TabContentsViewViews::CloseTab);
  }
  tab_contents_->SystemDragEnded();
}

views::internal::NativeWidgetDelegate*
    TabContentsViewViews::AsNativeWidgetDelegate() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsViewViews, views::Widget overrides:

views::FocusManager* TabContentsViewViews::GetFocusManager() {
  return const_cast<views::FocusManager*>(
      static_cast<const TabContentsViewViews*>(this)->GetFocusManager());
}

const views::FocusManager* TabContentsViewViews::GetFocusManager() const {
  const views::FocusManager* focus_manager = Widget::GetFocusManager();
  if (focus_manager) {
    // If |focus_manager| is non NULL, it means we have been reparented, in
    // which case |focus_manager_| may not be valid anymore.
    focus_manager_ = NULL;
    return focus_manager;
  }
  // TODO(jcampan): we should DCHECK on focus_manager_, as it should not be
  // NULL.  We are not doing it as it breaks some unit-tests.  We should
  // probably have an empty TabContentView implementation for the unit-tests,
  // that would prevent that code being executed in the unit-test case.
  // DCHECK(focus_manager_);
  return focus_manager_;
}

void TabContentsViewViews::OnNativeWidgetVisibilityChanged(bool visible) {
  views::Widget::OnNativeWidgetVisibilityChanged(visible);
  if (visible) {
    tab_contents_->ShowContents();
  } else {
    tab_contents_->HideContents();
  }
}

void TabContentsViewViews::OnNativeWidgetSizeChanged(
    const gfx::Size& new_size) {
  if (overlaid_view_)
    overlaid_view_->SetBounds(gfx::Rect(new_size));
  views::Widget::OnNativeWidgetSizeChanged(new_size);
}
