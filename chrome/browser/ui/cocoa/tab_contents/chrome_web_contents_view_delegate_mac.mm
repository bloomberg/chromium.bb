// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/chrome_web_contents_view_delegate_mac.h"

#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_delegate.h"
#include "chrome/browser/ui/cocoa/tab_contents/render_view_context_menu_mac.h"
#include "chrome/browser/ui/cocoa/tab_contents/web_drag_bookmark_handler_mac.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

ChromeWebContentsViewDelegateMac::ChromeWebContentsViewDelegateMac(
    content::WebContents* web_contents)
    : bookmark_handler_(new WebDragBookmarkHandlerMac),
      web_contents_(web_contents) {
}

ChromeWebContentsViewDelegateMac::~ChromeWebContentsViewDelegateMac() {
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
    const content::ContextMenuParams& params,
    content::ContextMenuSourceType type) {
  // The renderer may send the "show context menu" message multiple times, one
  // for each right click mouse event it receives. Normally, this doesn't happen
  // because mouse events are not forwarded once the context menu is showing.
  // However, there's a race - the context menu may not yet be showing when
  // the second mouse event arrives. In this case, |ShowContextMenu()| will
  // get called multiple times - if so, don't create another context menu.
  // TODO(asvitkine): Fix the renderer so that it doesn't do this.
  content::RenderWidgetHostView* widget_view =
      web_contents_->GetRenderWidgetHostView();
  if (widget_view && widget_view->IsShowingContextMenu())
    return;

  context_menu_.reset(
      new RenderViewContextMenuMac(web_contents_,
                                   params,
                                   web_contents_->GetContentNativeView()));
  context_menu_->Init();
}

namespace chrome {

content::WebContentsViewDelegate* CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return new ChromeWebContentsViewDelegateMac(web_contents);
}

}  // namespace chrome
