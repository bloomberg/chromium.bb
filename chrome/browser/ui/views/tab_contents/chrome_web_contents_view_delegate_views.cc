// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_delegate_views.h"

#include <utility>

#include "chrome/browser/defaults.h"
#include "chrome/browser/ui/aura/tab_contents/web_drag_bookmark_handler_aura.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "chrome/browser/ui/views/renderer_context_menu/render_view_context_menu_views.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/aura/window.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/widget/widget.h"

ChromeWebContentsViewDelegateViews::ChromeWebContentsViewDelegateViews(
    content::WebContents* web_contents)
    : ContextMenuDelegate(web_contents),
      web_contents_(web_contents) {
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

gfx::NativeWindow ChromeWebContentsViewDelegateViews::GetNativeWindow() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  return browser ? browser->window()->GetNativeWindow() : nullptr;
}

content::WebDragDestDelegate*
    ChromeWebContentsViewDelegateViews::GetDragDestDelegate() {
  // We install a chrome specific handler to intercept bookmark drags for the
  // bookmark manager/extension API.
  bookmark_handler_.reset(new WebDragBookmarkHandlerAura);
  return bookmark_handler_.get();
}

bool ChromeWebContentsViewDelegateViews::Focus() {
  SadTabHelper* sad_tab_helper = SadTabHelper::FromWebContents(web_contents_);
  if (sad_tab_helper) {
    SadTabView* sad_tab = static_cast<SadTabView*>(sad_tab_helper->sad_tab());
    if (sad_tab) {
      sad_tab->RequestFocus();
      return true;
    }
  }

  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_);
  if (manager && manager->IsDialogActive())
    manager->FocusTopmostDialog();

  return false;
}

void ChromeWebContentsViewDelegateViews::TakeFocus(bool reverse) {
  views::FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->AdvanceFocus(reverse);
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

std::unique_ptr<RenderViewContextMenuBase>
ChromeWebContentsViewDelegateViews::BuildMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  std::unique_ptr<RenderViewContextMenuBase> menu;
  content::RenderFrameHost* focused_frame = web_contents->GetFocusedFrame();
  // If the frame tree does not have a focused frame at this point, do not
  // bother creating RenderViewContextMenuViews.
  // This happens if the frame has navigated to a different page before
  // ContextMenu message was received by the current RenderFrameHost.
  if (focused_frame) {
    menu.reset(RenderViewContextMenuViews::Create(focused_frame, params));
    menu->Init();
  }
  return menu;
}

void ChromeWebContentsViewDelegateViews::ShowMenu(
    std::unique_ptr<RenderViewContextMenuBase> menu) {
  context_menu_ = std::move(menu);
  if (!context_menu_)
    return;

  context_menu_->Show();
}

void ChromeWebContentsViewDelegateViews::ShowContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  ShowMenu(
      BuildMenu(content::WebContents::FromRenderFrameHost(render_frame_host),
                params));
}

void ChromeWebContentsViewDelegateViews::SizeChanged(const gfx::Size& size) {
  SadTabHelper* sad_tab_helper = SadTabHelper::FromWebContents(web_contents_);
  if (!sad_tab_helper)
    return;
  SadTabView* sad_tab = static_cast<SadTabView*>(sad_tab_helper->sad_tab());
  if (sad_tab)
    sad_tab->GetWidget()->SetBounds(gfx::Rect(size));
}

aura::Window* ChromeWebContentsViewDelegateViews::GetActiveNativeView() {
  return web_contents_->GetFullscreenRenderWidgetHostView() ?
      web_contents_->GetFullscreenRenderWidgetHostView()->GetNativeView() :
      web_contents_->GetNativeView();
}

views::Widget* ChromeWebContentsViewDelegateViews::GetTopLevelWidget() {
  return views::Widget::GetTopLevelWidgetForNativeView(GetActiveNativeView());
}

views::FocusManager*
    ChromeWebContentsViewDelegateViews::GetFocusManager() {
  views::Widget* toplevel_widget = GetTopLevelWidget();
  return toplevel_widget ? toplevel_widget->GetFocusManager() : NULL;
}

void ChromeWebContentsViewDelegateViews::SetInitialFocus() {
  if (web_contents_->FocusLocationBarByDefault()) {
    if (web_contents_->GetDelegate())
      web_contents_->GetDelegate()->SetFocusToLocationBar(false);
  } else {
    web_contents_->Focus();
  }
}

namespace chrome {

content::WebContentsViewDelegate* CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return new ChromeWebContentsViewDelegateViews(web_contents);
}

}  // namespace chrome
