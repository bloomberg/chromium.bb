// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/web_contents_view_delegate_creator.h"

#include "athena/content/render_view_context_menu_impl.h"
#include "components/web_modal/popup_manager.h"
#include "components/web_modal/single_web_contents_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace athena {
namespace {

class WebContentsViewDelegateImpl : public content::WebContentsViewDelegate {
 public:
  explicit WebContentsViewDelegateImpl(content::WebContents* web_contents)
      : web_contents_(web_contents) {}
  virtual ~WebContentsViewDelegateImpl() {}

  virtual content::WebDragDestDelegate* GetDragDestDelegate() OVERRIDE {
    // TODO(oshima): crbug.com/401610
    return NULL;
  }

  virtual bool Focus() OVERRIDE {
    web_modal::PopupManager* popup_manager =
        web_modal::PopupManager::FromWebContents(web_contents_);
    if (popup_manager)
      popup_manager->WasFocused(web_contents_);
    return false;
  }

  virtual void TakeFocus(bool reverse) OVERRIDE {}
  virtual void StoreFocus() OVERRIDE {}
  virtual void RestoreFocus() OVERRIDE {}

  virtual void ShowContextMenu(
      content::RenderFrameHost* render_frame_host,
      const content::ContextMenuParams& params) OVERRIDE {
    ShowMenu(BuildMenu(
        content::WebContents::FromRenderFrameHost(render_frame_host), params));
  }

  virtual void SizeChanged(const gfx::Size& size) OVERRIDE {
    // TODO(oshima|sadrul): Implement this when sad_tab is componentized.
    // See c/b/ui/views/tab_contents/chrome_web_contents_view_delegate_views.cc
  }

  scoped_ptr<RenderViewContextMenuImpl> BuildMenu(
      content::WebContents* web_contents,
      const content::ContextMenuParams& params) {
    scoped_ptr<RenderViewContextMenuImpl> menu;
    content::RenderFrameHost* focused_frame = web_contents->GetFocusedFrame();
    // If the frame tree does not have a focused frame at this point, do not
    // bother creating RenderViewContextMenuViews.
    // This happens if the frame has navigated to a different page before
    // ContextMenu message was received by the current RenderFrameHost.
    if (focused_frame) {
      menu.reset(new RenderViewContextMenuImpl(focused_frame, params));
      menu->Init();
    }
    return menu.Pass();
  }

  void ShowMenu(scoped_ptr<RenderViewContextMenuImpl> menu) {
    context_menu_.reset(menu.release());

    if (!context_menu_.get())
      return;

    // Menus need a Widget to work. If we're not the active tab we won't
    // necessarily be in a widget.
    views::Widget* top_level_widget = GetTopLevelWidget();
    if (!top_level_widget)
      return;

    const content::ContextMenuParams& params = context_menu_->params();
    // Don't show empty menus.
    if (context_menu_->menu_model().GetItemCount() == 0)
      return;

    gfx::Point screen_point(params.x, params.y);

    // Convert from target window coordinates to root window coordinates.
    aura::Window* target_window = GetActiveNativeView();
    aura::Window* root_window = target_window->GetRootWindow();
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root_window);
    if (screen_position_client) {
      screen_position_client->ConvertPointToScreen(target_window,
                                                   &screen_point);
    }
    // Enable recursive tasks on the message loop so we can get updates while
    // the context menu is being displayed.
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());
    context_menu_->RunMenuAt(
        top_level_widget, screen_point, params.source_type);
  }

  aura::Window* GetActiveNativeView() {
    return web_contents_->GetFullscreenRenderWidgetHostView()
               ? web_contents_->GetFullscreenRenderWidgetHostView()
                     ->GetNativeView()
               : web_contents_->GetNativeView();
  }

  views::Widget* GetTopLevelWidget() {
    return views::Widget::GetTopLevelWidgetForNativeView(GetActiveNativeView());
  }

  views::FocusManager* GetFocusManager() {
    views::Widget* toplevel_widget = GetTopLevelWidget();
    return toplevel_widget ? toplevel_widget->GetFocusManager() : NULL;
  }

  void SetInitialFocus() {
    if (web_contents_->FocusLocationBarByDefault()) {
      if (web_contents_->GetDelegate())
        web_contents_->GetDelegate()->SetFocusToLocationBar(false);
    } else {
      web_contents_->Focus();
    }
  }
  scoped_ptr<RenderViewContextMenuImpl> context_menu_;
  content::WebContents* web_contents_;
  DISALLOW_COPY_AND_ASSIGN(WebContentsViewDelegateImpl);
};

}  // namespace

content::WebContentsViewDelegate* CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return new WebContentsViewDelegateImpl(web_contents);
}

}  // namespace athena

namespace web_modal {

SingleWebContentsDialogManager*
WebContentsModalDialogManager::CreateNativeWebModalManager(
    NativeWebContentsModalDialog dialog,
    SingleWebContentsDialogManagerDelegate* native_delegate) {
  // TODO(oshima): Investigate if we need to implement this.
  NOTREACHED();
  return NULL;
}

}  // namespace web_modal
