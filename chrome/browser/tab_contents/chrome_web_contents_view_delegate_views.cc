// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/chrome_web_contents_view_delegate_views.h"

#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/views/tab_contents/render_view_context_menu_views.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "chrome/browser/tab_contents/web_drag_bookmark_handler_aura.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#else
#include "chrome/browser/tab_contents/web_drag_bookmark_handler_win.h"
#endif

ChromeWebContentsViewDelegateViews::ChromeWebContentsViewDelegateViews(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  last_focused_view_storage_id_ =
      views::ViewStorage::GetInstance()->CreateStorageID();
}

ChromeWebContentsViewDelegateViews::~ChromeWebContentsViewDelegateViews() {
  // Makes sure to remove any stored view we may still have in the ViewStorage.
  //
  // It is possible the view went away before us, so we only do this if the
  // view is registered.
  views::ViewStorage* view_storage = views::ViewStorage::GetInstance();
  if (view_storage->RetrieveView(last_focused_view_storage_id_) != NULL)
    view_storage->RemoveView(last_focused_view_storage_id_);
}

content::WebDragDestDelegate*
    ChromeWebContentsViewDelegateViews::GetDragDestDelegate() {
  // We install a chrome specific handler to intercept bookmark drags for the
  // bookmark manager/extension API.
#if defined(USE_AURA)
  bookmark_handler_.reset(new WebDragBookmarkHandlerAura);
#else
  bookmark_handler_.reset(new WebDragBookmarkHandlerWin);
#endif
  return bookmark_handler_.get();
}

bool ChromeWebContentsViewDelegateViews::Focus() {
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents_);
  if (wrapper) {
      views::Widget* sad_tab = wrapper->sad_tab_helper()->sad_tab();
    if (sad_tab) {
      sad_tab->GetContentsView()->RequestFocus();
      return true;
    }

    // TODO(erg): WebContents used to own constrained windows, which is why
    // this is here. Eventually this should be ported to a containing view
    // specializing in constrained window management.
    ConstrainedWindowTabHelper* helper =
        wrapper->constrained_window_tab_helper();
    if (helper->constrained_window_count() > 0) {
      ConstrainedWindow* window = *helper->constrained_window_begin();
      DCHECK(window);
      window->FocusConstrainedWindow();
      return true;
    }
  }
  return false;
}

void ChromeWebContentsViewDelegateViews::TakeFocus(bool reverse) {
  GetFocusManager()->AdvanceFocus(reverse);
}

void ChromeWebContentsViewDelegateViews::StoreFocus() {
  views::ViewStorage* view_storage = views::ViewStorage::GetInstance();

  if (view_storage->RetrieveView(last_focused_view_storage_id_) != NULL)
    view_storage->RemoveView(last_focused_view_storage_id_);

  if (!GetFocusManager())
    return;
  views::View* focused_view = GetFocusManager()->GetFocusedView();
  if (focused_view)
    view_storage->StoreView(last_focused_view_storage_id_, focused_view);
}

void ChromeWebContentsViewDelegateViews::RestoreFocus() {
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

void ChromeWebContentsViewDelegateViews::ShowContextMenu(
    const content::ContextMenuParams& params) {
  context_menu_.reset(new RenderViewContextMenuViews(web_contents_, params));
  context_menu_->Init();

  // Don't show empty menus.
  if (context_menu_->menu_model().GetItemCount() == 0)
    return;

  gfx::Point screen_point(params.x, params.y);

#if defined(USE_AURA)
  // Convert from content coordinates to window coordinates.
  aura::Window* web_contents_window =
      web_contents_->GetView()->GetNativeView();
  aura::RootWindow* root_window = web_contents_window->GetRootWindow();
  aura::Window::ConvertPointToWindow(web_contents_window, root_window,
                                     &screen_point);

  // If we are on the desktop, transform the data from our toplevel window
  // coordinates to screen coordinates.
  aura::Window* toplevel_window =
      web_contents_->GetView()->GetTopLevelNativeWindow();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(toplevel_window);
  if (screen_position_client)
    screen_position_client->ConvertToScreenPoint(&screen_point);
#else
  POINT temp = screen_point.ToPOINT();
  ClientToScreen(web_contents_->GetView()->GetNativeView(), &temp);
  screen_point = temp;
#endif

  // Enable recursive tasks on the message loop so we can get updates while
  // the context menu is being displayed.
  MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
  context_menu_->RunMenuAt(GetTopLevelWidget(), screen_point);
}

void ChromeWebContentsViewDelegateViews::SizeChanged(const gfx::Size& size) {
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents_);
  if (!wrapper)
    return;
  views::Widget* sad_tab = wrapper->sad_tab_helper()->sad_tab();
  if (sad_tab)
    sad_tab->SetBounds(gfx::Rect(size));
}

views::Widget* ChromeWebContentsViewDelegateViews::GetTopLevelWidget() {
  gfx::NativeWindow top_level_window =
      web_contents_->GetView()->GetTopLevelNativeWindow();
  if (!top_level_window)
    return NULL;
  return views::Widget::GetWidgetForNativeWindow(top_level_window);
}

views::FocusManager*
    ChromeWebContentsViewDelegateViews::GetFocusManager() {
  views::Widget* toplevel_widget = GetTopLevelWidget();
  return toplevel_widget ? toplevel_widget->GetFocusManager() : NULL;
}

void ChromeWebContentsViewDelegateViews::SetInitialFocus() {
  if (web_contents_->FocusLocationBarByDefault()) {
    web_contents_->SetFocusToLocationBar(false);
  } else {
    web_contents_->GetView()->Focus();
  }
}
