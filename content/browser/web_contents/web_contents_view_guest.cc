// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_guest.h"

#include "build/build_config.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_guest.h"
#include "content/browser/web_contents/interstitial_page_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "webkit/glue/webdropdata.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;

namespace content {

WebContentsViewGuest::WebContentsViewGuest(
    WebContentsImpl* web_contents,
    BrowserPluginGuest* guest,
    WebContentsViewPort* platform_view)
    : web_contents_(web_contents),
      guest_(guest),
      platform_view_(platform_view) {
}

WebContentsViewGuest::~WebContentsViewGuest() {
}

gfx::NativeView WebContentsViewGuest::GetNativeView() const {
  return platform_view_->GetNativeView();
}

gfx::NativeView WebContentsViewGuest::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv)
    return NULL;
  return rwhv->GetNativeView();
}

gfx::NativeWindow WebContentsViewGuest::GetTopLevelNativeWindow() const {
  return guest_->embedder_web_contents()->GetView()->GetTopLevelNativeWindow();
}

void WebContentsViewGuest::GetContainerBounds(gfx::Rect* out) const {
  platform_view_->GetContainerBounds(out);
}

void WebContentsViewGuest::SizeContents(const gfx::Size& size) {
  platform_view_->SizeContents(size);
}

void WebContentsViewGuest::SetInitialFocus() {
  platform_view_->SetInitialFocus();
}

gfx::Rect WebContentsViewGuest::GetViewBounds() const {
  return platform_view_->GetViewBounds();
}

#if defined(OS_MACOSX)
void WebContentsViewGuest::SetAllowOverlappingViews(bool overlapping) {
  platform_view_->SetAllowOverlappingViews(overlapping);
}
#endif

void WebContentsViewGuest::CreateView(const gfx::Size& initial_size,
                                      gfx::NativeView context) {
  platform_view_->CreateView(initial_size, context);
}

RenderWidgetHostView* WebContentsViewGuest::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->GetView()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->GetView();
  }

  RenderWidgetHostView* platform_widget = NULL;
  platform_widget = platform_view_->CreateViewForWidget(render_widget_host);

  RenderWidgetHostView* view = new RenderWidgetHostViewGuest(
      render_widget_host,
      guest_,
      platform_widget);

  return view;
}

RenderWidgetHostView* WebContentsViewGuest::CreateViewForPopupWidget(
    RenderWidgetHost* render_widget_host) {
  return RenderWidgetHostViewPort::CreateViewForWidget(render_widget_host);
}

void WebContentsViewGuest::SetPageTitle(const string16& title) {
}

void WebContentsViewGuest::RenderViewCreated(RenderViewHost* host) {
  platform_view_->RenderViewCreated(host);
}

void WebContentsViewGuest::RenderViewSwappedIn(RenderViewHost* host) {
  platform_view_->RenderViewSwappedIn(host);
}

void WebContentsViewGuest::SetOverscrollControllerEnabled(bool enabled) {
  // This should never override the setting of the embedder view.
}

#if defined(OS_MACOSX)
bool WebContentsViewGuest::IsEventTracking() const {
  return false;
}

void WebContentsViewGuest::CloseTabAfterEventTracking() {
}
#endif

WebContents* WebContentsViewGuest::web_contents() {
  return web_contents_;
}

void WebContentsViewGuest::RestoreFocus() {
  platform_view_->RestoreFocus();
}

void WebContentsViewGuest::OnTabCrashed(base::TerminationStatus status,
                                        int error_code) {
}

void WebContentsViewGuest::Focus() {
  platform_view_->Focus();
}

void WebContentsViewGuest::StoreFocus() {
  platform_view_->StoreFocus();
}

WebDropData* WebContentsViewGuest::GetDropData() const {
  NOTIMPLEMENTED();
  return NULL;
}

void WebContentsViewGuest::UpdateDragCursor(WebDragOperation operation) {
  NOTIMPLEMENTED();
}

void WebContentsViewGuest::GotFocus() {
}

void WebContentsViewGuest::TakeFocus(bool reverse) {
}

void WebContentsViewGuest::ShowContextMenu(
    const ContextMenuParams& params,
    ContextMenuSourceType type) {
  NOTIMPLEMENTED();
}

void WebContentsViewGuest::ShowPopupMenu(const gfx::Rect& bounds,
                                         int item_height,
                                         double item_font_size,
                                         int selected_item,
                                         const std::vector<WebMenuItem>& items,
                                         bool right_aligned,
                                         bool allow_multiple_selection) {
  // External popup menus are only used on Mac and Android.
  NOTIMPLEMENTED();
}

void WebContentsViewGuest::StartDragging(
    const WebDropData& drop_data,
    WebDragOperationsMask ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const DragEventSourceInfo& event_info) {
  NOTIMPLEMENTED();
}

}  // namespace content
