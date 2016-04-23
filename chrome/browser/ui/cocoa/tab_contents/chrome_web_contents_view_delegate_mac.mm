// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/chrome_web_contents_view_delegate_mac.h"

#include <utility>

#include "base/profiler/scoped_tracker.h"
#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac.h"
#include "chrome/browser/ui/cocoa/tab_contents/web_drag_bookmark_handler_mac.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

ChromeWebContentsViewDelegateMac::ChromeWebContentsViewDelegateMac(
    content::WebContents* web_contents)
    : ContextMenuDelegate(web_contents),
      bookmark_handler_(new WebDragBookmarkHandlerMac),
      web_contents_(web_contents) {
}

ChromeWebContentsViewDelegateMac::~ChromeWebContentsViewDelegateMac() {
}

gfx::NativeWindow ChromeWebContentsViewDelegateMac::GetNativeWindow() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  return browser ? browser->window()->GetNativeWindow() : nullptr;
}

NSObject<RenderWidgetHostViewMacDelegate>*
ChromeWebContentsViewDelegateMac::CreateRenderWidgetHostViewDelegate(
    content::RenderWidgetHost* render_widget_host) {
  return [[ChromeRenderWidgetHostViewMacDelegate alloc]
      initWithRenderWidgetHost:render_widget_host];
}

content::WebDragDestDelegate*
    ChromeWebContentsViewDelegateMac::GetDragDestDelegate() {
  return bookmark_handler_.get();
}

void ChromeWebContentsViewDelegateMac::ShowContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // TODO(erikchen): Remove ScopedTracker below once crbug.com/458401 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "458401 ChromeWebContentsViewDelegateMac::ShowContextMenu"));
  ShowMenu(
      BuildMenu(content::WebContents::FromRenderFrameHost(render_frame_host),
                params));
}

void ChromeWebContentsViewDelegateMac::ShowMenu(
    std::unique_ptr<RenderViewContextMenuBase> menu) {
  // TODO(erikchen): Remove ScopedTracker below once crbug.com/458401 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "458401 ChromeWebContentsViewDelegateMac::ShowMenu"));

  context_menu_ = std::move(menu);
  if (!context_menu_.get())
    return;

  // The renderer may send the "show context menu" message multiple times, one
  // for each right click mouse event it receives. Normally, this doesn't happen
  // because mouse events are not forwarded once the context menu is showing.
  // However, there's a race - the context menu may not yet be showing when
  // the second mouse event arrives. In this case, |ShowContextMenu()| will
  // get called multiple times - if so, don't create another context menu.
  // TODO(asvitkine): Fix the renderer so that it doesn't do this.
  content::RenderWidgetHostView* widget_view = GetActiveRenderWidgetHostView();
  if (widget_view && widget_view->IsShowingContextMenu())
    return;

  context_menu_->Show();
}

std::unique_ptr<RenderViewContextMenuBase>
ChromeWebContentsViewDelegateMac::BuildMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  // TODO(erikchen): Remove ScopedTracker below once crbug.com/458401 is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "458401 ChromeWebContentsViewDelegateMac::BuildMenu"));
  std::unique_ptr<RenderViewContextMenuBase> menu;
  content::RenderFrameHost* focused_frame = web_contents->GetFocusedFrame();
  // If the frame tree does not have a focused frame at this point, do not
  // bother creating RenderViewContextMenuMac.
  // This happens if the frame has navigated to a different page before
  // ContextMenu message was received by the current RenderFrameHost.
  if (focused_frame) {
    content::RenderWidgetHostView* widget_view =
        GetActiveRenderWidgetHostView();

    // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/458401
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile2(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "458401 ChromeWebContentsViewDelegateMac::BuildMenu::MakeMenu"));
    menu.reset(new RenderViewContextMenuMac(
        focused_frame, params, widget_view->GetNativeView()));

    // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/458401
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile3(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "458401 ChromeWebContentsViewDelegateMac::BuildMenu::InitMenu"));
    menu->Init();
  }

  return menu;
}

content::RenderWidgetHostView*
ChromeWebContentsViewDelegateMac::GetActiveRenderWidgetHostView() {
  return web_contents_->GetFullscreenRenderWidgetHostView() ?
      web_contents_->GetFullscreenRenderWidgetHostView() :
      web_contents_->GetTopLevelRenderWidgetHostView();
}

namespace chrome {

content::WebContentsViewDelegate* CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return new ChromeWebContentsViewDelegateMac(web_contents);
}

}  // namespace chrome
