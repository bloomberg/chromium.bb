// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/tab_contents_view_views.h"

#include <vector>

#include "base/time.h"
#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view.h"
#include "chrome/browser/ui/views/tab_contents/render_view_context_menu_views.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
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
using content::RenderViewHost;
using content::RenderWidgetHostView;
using content::WebContents;
using content::WebContentsViewDelegate;

TabContentsViewViews::TabContentsViewViews(WebContents* web_contents,
                                           WebContentsViewDelegate* delegate)
    : web_contents_(web_contents),
      native_tab_contents_view_(NULL),
      close_tab_after_drag_ends_(false),
      delegate_(delegate) {
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

void TabContentsViewViews::CreateView(const gfx::Size& initial_size) {
  native_tab_contents_view_ =
      NativeTabContentsView::CreateNativeTabContentsView(this);
  native_tab_contents_view_->InitNativeTabContentsView();
}

RenderWidgetHostView* TabContentsViewViews::CreateViewForWidget(
    content::RenderWidgetHost* render_widget_host) {
  if (render_widget_host->GetView()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->GetView();
  }

  return native_tab_contents_view_->CreateRenderWidgetHostView(
      render_widget_host);
}

gfx::NativeView TabContentsViewViews::GetNativeView() const {
  return Widget::GetNativeView();
}

gfx::NativeView TabContentsViewViews::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
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
  if (web_contents_->GetInterstitialPage()) {
    web_contents_->GetInterstitialPage()->Focus();
    return;
  }

  views::Widget* sad_tab = GetSadTab();
  if (sad_tab) {
    sad_tab->GetContentsView()->RequestFocus();
    return;
  }

  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents_);
  if (wrapper) {
    // TODO(erg): WebContents used to own constrained windows, which is why
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
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
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
  if (web_contents_->FocusLocationBarByDefault())
    web_contents_->SetFocusToLocationBar(false);
  else
    Focus();
}

void TabContentsViewViews::StoreFocus() {
  views::ViewStorage* view_storage = views::ViewStorage::GetInstance();

  if (view_storage->RetrieveView(last_focused_view_storage_id_) != NULL)
    view_storage->RemoveView(last_focused_view_storage_id_);

  views::View* focused_view = GetFocusManager()->GetFocusedView();
  if (focused_view)
    view_storage->StoreView(last_focused_view_storage_id_, focused_view);
}

void TabContentsViewViews::RestoreFocus() {
  views::ViewStorage* view_storage = views::ViewStorage::GetInstance();
  views::View* last_focused_view =
      view_storage->RetrieveView(last_focused_view_storage_id_);

  if (!last_focused_view) {
    SetInitialFocus();
  } else {
    if (last_focused_view->IsFocusable() &&
        GetFocusManager()->ContainsView(last_focused_view)) {
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

void TabContentsViewViews::UpdateDragCursor(WebDragOperation operation) {
  native_tab_contents_view_->SetDragCursor(operation);
}

void TabContentsViewViews::GotFocus() {
  if (web_contents_->GetDelegate())
    web_contents_->GetDelegate()->WebContentsFocused(web_contents_);
}

void TabContentsViewViews::TakeFocus(bool reverse) {
  if (web_contents_->GetDelegate() &&
      !web_contents_->GetDelegate()->TakeFocus(reverse)) {
    GetFocusManager()->AdvanceFocus(reverse);
  }
}

void TabContentsViewViews::CloseTab() {
  RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  rvh->GetDelegate()->Close(rvh);
}

views::Widget* TabContentsViewViews::GetSadTab() const {
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents_);
  return wrapper ? wrapper->sad_tab_helper()->sad_tab() : NULL;
}

void TabContentsViewViews::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  web_contents_view_helper_.CreateNewWindow(web_contents_, route_id, params);
}

void TabContentsViewViews::CreateNewWidget(
    int route_id, WebKit::WebPopupType popup_type) {
  web_contents_view_helper_.CreateNewWidget(web_contents_,
                                            route_id,
                                            false,
                                            popup_type);
}

void TabContentsViewViews::CreateNewFullscreenWidget(int route_id) {
  web_contents_view_helper_.CreateNewWidget(web_contents_,
                                            route_id,
                                            true,
                                            WebKit::WebPopupTypeNone);
}

void TabContentsViewViews::ShowCreatedWindow(int route_id,
                                             WindowOpenDisposition disposition,
                                             const gfx::Rect& initial_pos,
                                             bool user_gesture) {
  web_contents_view_helper_.ShowCreatedWindow(
      web_contents_, route_id, disposition, initial_pos, user_gesture);
}

void TabContentsViewViews::ShowCreatedWidget(
    int route_id, const gfx::Rect& initial_pos) {
  web_contents_view_helper_.ShowCreatedWidget(web_contents_,
                                              route_id,
                                              false,
                                              initial_pos);
}

void TabContentsViewViews::ShowCreatedFullscreenWidget(int route_id) {
  web_contents_view_helper_.ShowCreatedWidget(web_contents_,
                                              route_id,
                                              true,
                                              gfx::Rect());
}

void TabContentsViewViews::ShowContextMenu(
    const content::ContextMenuParams& params) {
  // Allow delegates to handle the context menu operation first.
  if (web_contents_->GetDelegate() &&
      web_contents_->GetDelegate()->HandleContextMenu(params)) {
    return;
  }

  context_menu_.reset(new RenderViewContextMenuViews(web_contents_, params));
  context_menu_->Init();

  // Don't show empty menus.
  if (context_menu_->menu_model().GetItemCount() == 0)
    return;

  gfx::Point screen_point(params.x, params.y);
  views::View::ConvertPointToScreen(GetRootView(), &screen_point);

  // Enable recursive tasks on the message loop so we can get updates while
  // the context menu is being displayed.
  MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
  context_menu_->RunMenuAt(GetTopLevelWidget(), screen_point);
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

WebContents* TabContentsViewViews::GetWebContents() {
  return web_contents_;
}

void TabContentsViewViews::OnNativeTabContentsViewShown() {
  web_contents_->ShowContents();
}

void TabContentsViewViews::OnNativeTabContentsViewHidden() {
  web_contents_->HideContents();
}

void TabContentsViewViews::OnNativeTabContentsViewSized(const gfx::Size& size) {
  if (web_contents_->GetInterstitialPage())
    web_contents_->GetInterstitialPage()->SetSize(size);
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
}

void TabContentsViewViews::OnNativeTabContentsViewWheelZoom(bool zoom_in) {
  if (web_contents_->GetDelegate())
    web_contents_->GetDelegate()->ContentsZoomChange(zoom_in);
}

void TabContentsViewViews::OnNativeTabContentsViewMouseDown() {
  // Make sure this WebContents is activated when it is clicked on.
  if (web_contents_->GetDelegate())
    web_contents_->GetDelegate()->ActivateContents(web_contents_);
}

void TabContentsViewViews::OnNativeTabContentsViewMouseMove(bool motion) {
  // Let our delegate know that the mouse moved (useful for resetting status
  // bubble state).
  if (web_contents_->GetDelegate()) {
    web_contents_->GetDelegate()->ContentsMouseEvent(
        web_contents_, gfx::Screen::GetCursorScreenPoint(), motion);
  }
}

void TabContentsViewViews::OnNativeTabContentsViewDraggingEnded() {
  if (close_tab_after_drag_ends_) {
    close_tab_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(0),
                           this, &TabContentsViewViews::CloseTab);
  }
  web_contents_->SystemDragEnded();
}

views::internal::NativeWidgetDelegate*
    TabContentsViewViews::AsNativeWidgetDelegate() {
  return this;
}

content::WebDragDestDelegate* TabContentsViewViews::GetDragDestDelegate() {
  if (delegate_.get())
    return delegate_->GetDragDestDelegate();
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsViewViews, views::Widget overrides:

void TabContentsViewViews::OnNativeWidgetVisibilityChanged(bool visible) {
  views::Widget::OnNativeWidgetVisibilityChanged(visible);
  if (visible) {
    web_contents_->ShowContents();
  } else {
    web_contents_->HideContents();
  }
}

void TabContentsViewViews::OnNativeWidgetSizeChanged(
    const gfx::Size& new_size) {
  views::Widget* sad_tab = GetSadTab();
  if (sad_tab)
    sad_tab->SetBounds(gfx::Rect(new_size));
  views::Widget::OnNativeWidgetSizeChanged(new_size);
}
