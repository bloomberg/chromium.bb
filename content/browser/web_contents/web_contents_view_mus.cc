// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_mus.h"

#include "build/build_config.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "content/browser/renderer_host/render_widget_host_view_mus.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"

using blink::WebDragOperation;
using blink::WebDragOperationsMask;

namespace content {

WebContentsViewMus::WebContentsViewMus(
    WebContentsImpl* web_contents,
    mus::Window* parent_window,
    scoped_ptr<WebContentsView> platform_view,
    RenderViewHostDelegateView** delegate_view)
    : web_contents_(web_contents),
      platform_view_(platform_view.Pass()),
      platform_view_delegate_view_(*delegate_view) {
  DCHECK(parent_window);
  *delegate_view = this;
  mus::Window* window = parent_window->connection()->NewWindow();
  window->SetVisible(true);
  window->SetBounds(gfx::Rect(300, 300));
  parent_window->AddChild(window);
  window_.reset(new mus::ScopedWindowPtr(window));
}

WebContentsViewMus::~WebContentsViewMus() {}

gfx::NativeView WebContentsViewMus::GetNativeView() const {
  return platform_view_->GetNativeView();
}

gfx::NativeView WebContentsViewMus::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv)
    return NULL;
  return rwhv->GetNativeView();
}

gfx::NativeWindow WebContentsViewMus::GetTopLevelNativeWindow() const {
  return platform_view_->GetTopLevelNativeWindow();
}

void WebContentsViewMus::GetContainerBounds(gfx::Rect* out) const {
  // TODO(fsamuel): Get the position right.
  out->set_size(size_);
}

void WebContentsViewMus::SizeContents(const gfx::Size& size) {
  size_ = size;
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
  window_->window()->SetBounds(gfx::Rect(size));
}

void WebContentsViewMus::SetInitialFocus() {
  platform_view_->SetInitialFocus();
}

gfx::Rect WebContentsViewMus::GetViewBounds() const {
  return gfx::Rect(size_);
}

#if defined(OS_MACOSX)
void WebContentsViewMus::SetAllowOtherViews(bool allow) {
  platform_view_->SetAllowOtherViews(allow);
}

bool WebContentsViewMus::GetAllowOtherViews() const {
  return platform_view_->GetAllowOtherViews();
}
#endif

void WebContentsViewMus::CreateView(const gfx::Size& initial_size,
                                    gfx::NativeView context) {
  platform_view_->CreateView(initial_size, context);
  size_ = initial_size;
}

RenderWidgetHostViewBase* WebContentsViewMus::CreateViewForWidget(
    RenderWidgetHost* render_widget_host,
    bool is_guest_view_hack) {
  RenderWidgetHostViewBase* platform_widget =
      platform_view_->CreateViewForWidget(render_widget_host, true);

  return new RenderWidgetHostViewMus(
      window_->window(), RenderWidgetHostImpl::From(render_widget_host),
      platform_widget->GetWeakPtr());
}

RenderWidgetHostViewBase* WebContentsViewMus::CreateViewForPopupWidget(
    RenderWidgetHost* render_widget_host) {
  return platform_view_->CreateViewForPopupWidget(render_widget_host);
}

void WebContentsViewMus::SetPageTitle(const base::string16& title) {}

void WebContentsViewMus::RenderViewCreated(RenderViewHost* host) {
  platform_view_->RenderViewCreated(host);
}

void WebContentsViewMus::RenderViewSwappedIn(RenderViewHost* host) {
  platform_view_->RenderViewSwappedIn(host);
}

void WebContentsViewMus::SetOverscrollControllerEnabled(bool enabled) {
  // This should never override the setting of the embedder view.
}

#if defined(OS_MACOSX)
bool WebContentsViewMus::IsEventTracking() const {
  return false;
}

void WebContentsViewMus::CloseTabAfterEventTracking() {}
#endif

WebContents* WebContentsViewMus::web_contents() {
  return web_contents_;
}

void WebContentsViewMus::RestoreFocus() {
  platform_view_->RestoreFocus();
}

void WebContentsViewMus::Focus() {
  platform_view_->Focus();
}

void WebContentsViewMus::StoreFocus() {
  platform_view_->StoreFocus();
}

DropData* WebContentsViewMus::GetDropData() const {
  NOTIMPLEMENTED();
  return NULL;
}

void WebContentsViewMus::UpdateDragCursor(WebDragOperation operation) {
  // TODO(fsamuel): Implement cursor in Mus.
}

void WebContentsViewMus::GotFocus() {}

void WebContentsViewMus::TakeFocus(bool reverse) {}

void WebContentsViewMus::ShowContextMenu(RenderFrameHost* render_frame_host,
                                         const ContextMenuParams& params) {
  platform_view_delegate_view_->ShowContextMenu(render_frame_host, params);
}

void WebContentsViewMus::StartDragging(const DropData& drop_data,
                                       WebDragOperationsMask ops,
                                       const gfx::ImageSkia& image,
                                       const gfx::Vector2d& image_offset,
                                       const DragEventSourceInfo& event_info) {
  // TODO(fsamuel): Implement drag and drop.
}

}  // namespace content
