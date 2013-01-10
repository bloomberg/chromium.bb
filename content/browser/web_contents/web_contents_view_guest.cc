// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_guest.h"

#include "build/build_config.h"
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
    BrowserPluginGuest* guest)
    : web_contents_(web_contents),
      guest_(guest) {
}

WebContentsViewGuest::~WebContentsViewGuest() {
}

void WebContentsViewGuest::CreateView(const gfx::Size& initial_size,
                                      gfx::NativeView context) {
  requested_size_ = initial_size;
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

  RenderWidgetHostView* view = new RenderWidgetHostViewGuest(
      render_widget_host,
      guest_);

  return view;
}

RenderWidgetHostView* WebContentsViewGuest::CreateViewForPopupWidget(
    RenderWidgetHost* render_widget_host) {
  return RenderWidgetHostViewPort::CreateViewForWidget(render_widget_host);
}

gfx::NativeView WebContentsViewGuest::GetNativeView() const {
  return NULL;
}

gfx::NativeView WebContentsViewGuest::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv)
    return NULL;
  return rwhv->GetNativeView();
}

gfx::NativeWindow WebContentsViewGuest::GetTopLevelNativeWindow() const {
  return NULL;
}

void WebContentsViewGuest::GetContainerBounds(gfx::Rect* out) const {
  out->SetRect(0, 0, requested_size_.width(), requested_size_.height());
}

void WebContentsViewGuest::SizeContents(const gfx::Size& size) {
  requested_size_ = size;
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
}

void WebContentsViewGuest::SetInitialFocus() {
  if (web_contents_->FocusLocationBarByDefault())
    web_contents_->SetFocusToLocationBar(false);
  else
    Focus();
}

gfx::Rect WebContentsViewGuest::GetViewBounds() const {
  gfx::Rect rect(0, 0, requested_size_.width(), requested_size_.height());
  return rect;
}

#if defined(OS_MACOSX)
void WebContentsViewGuest::SetAllowOverlappingViews(bool overlapping) {
  NOTIMPLEMENTED();
}
#endif

WebContents* WebContentsViewGuest::web_contents() {
  return web_contents_;
}

void WebContentsViewGuest::RenderViewCreated(RenderViewHost* host) {
}

bool WebContentsViewGuest::IsEventTracking() const {
  return false;
}

void WebContentsViewGuest::RestoreFocus() {
  SetInitialFocus();
}

void WebContentsViewGuest::SetPageTitle(const string16& title) {
  NOTIMPLEMENTED();
}

void WebContentsViewGuest::OnTabCrashed(base::TerminationStatus status,
                                        int error_code) {
  NOTIMPLEMENTED();
}

void WebContentsViewGuest::Focus() {
  NOTIMPLEMENTED();
}

void WebContentsViewGuest::StoreFocus() {
  NOTIMPLEMENTED();
}

WebDropData* WebContentsViewGuest::GetDropData() const {
  NOTIMPLEMENTED();
  return NULL;
}

void WebContentsViewGuest::CloseTabAfterEventTracking() {
  NOTIMPLEMENTED();
}

void WebContentsViewGuest::UpdateDragCursor(WebDragOperation operation) {
  NOTIMPLEMENTED();
}

void WebContentsViewGuest::GotFocus() {
  NOTIMPLEMENTED();
}

void WebContentsViewGuest::TakeFocus(bool reverse) {
  NOTIMPLEMENTED();
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
