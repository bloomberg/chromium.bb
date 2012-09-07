// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_android.h"

#include "base/logging.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {
WebContentsView* CreateWebContentsView(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate,
    RenderViewHostDelegateView** render_view_host_delegate_view) {
  WebContentsViewAndroid* rv = new WebContentsViewAndroid(
      web_contents, delegate);
  *render_view_host_delegate_view = rv;
  return rv;
}

WebContentsViewAndroid::WebContentsViewAndroid(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate)
    : web_contents_(web_contents),
      delegate_(delegate) {
}

WebContentsViewAndroid::~WebContentsViewAndroid() {
}

void WebContentsViewAndroid::SetContentViewCore(
    ContentViewCoreImpl* content_view_core) {
  content_view_core_ = content_view_core;
  RenderWidgetHostViewAndroid* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
      web_contents_->GetRenderWidgetHostView());
  if (rwhv)
    rwhv->SetContentViewCore(content_view_core_);
  if (web_contents_->ShowingInterstitialPage()) {
    NOTIMPLEMENTED() << "not upstreamed yet";
  }
}

void WebContentsViewAndroid::CreateView(const gfx::Size& initial_size) {
}

RenderWidgetHostView* WebContentsViewAndroid::CreateViewForWidget(
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
  // Note that while this instructs the render widget host to reference
  // |native_view_|, this has no effect without also instructing the
  // native view (i.e. ContentView) how to obtain a reference to this widget in
  // order to paint it. See ContentView::GetRenderWidgetHostViewAndroid for an
  // example of how this is achieved for InterstitialPages.
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(render_widget_host);
  return new RenderWidgetHostViewAndroid(rwhi, content_view_core_);
}

gfx::NativeView WebContentsViewAndroid::GetNativeView() const {
  return content_view_core_;
}

gfx::NativeView WebContentsViewAndroid::GetContentNativeView() const {
  return content_view_core_;
}

gfx::NativeWindow WebContentsViewAndroid::GetTopLevelNativeWindow() const {
  return content_view_core_;
}

void WebContentsViewAndroid::GetContainerBounds(gfx::Rect* out) const {
  if (content_view_core_)
    *out = content_view_core_->GetBounds();
}

void WebContentsViewAndroid::SetPageTitle(const string16& title) {
  if (content_view_core_)
    content_view_core_->SetTitle(title);
}

void WebContentsViewAndroid::OnTabCrashed(base::TerminationStatus status,
                                          int error_code) {
  NOTIMPLEMENTED() << "not upstreamed yet";
}

void WebContentsViewAndroid::SizeContents(const gfx::Size& size) {
  // TODO(klobag): Do we need to do anything else?
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
}

void WebContentsViewAndroid::RenderViewCreated(RenderViewHost* host) {
}

void WebContentsViewAndroid::Focus() {
  if (web_contents_->ShowingInterstitialPage())
    web_contents_->GetInterstitialPage()->Focus();
  else
    web_contents_->GetRenderWidgetHostView()->Focus();
}

void WebContentsViewAndroid::SetInitialFocus() {
  if (web_contents_->FocusLocationBarByDefault())
    web_contents_->SetFocusToLocationBar(false);
  else
    Focus();
}

void WebContentsViewAndroid::StoreFocus() {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::RestoreFocus() {
  NOTIMPLEMENTED();
}

bool WebContentsViewAndroid::IsDoingDrag() const {
  NOTIMPLEMENTED();
  return false;
}

void WebContentsViewAndroid::CancelDragAndCloseTab() {
  NOTIMPLEMENTED();
}

WebDropData* WebContentsViewAndroid::GetDropData() const {
  NOTIMPLEMENTED();
  return NULL;
}

bool WebContentsViewAndroid::IsEventTracking() const {
  NOTIMPLEMENTED();
  return false;
}

void WebContentsViewAndroid::CloseTabAfterEventTracking() {
  NOTIMPLEMENTED();
}

gfx::Rect WebContentsViewAndroid::GetViewBounds() const {
  if (content_view_core_)
    return content_view_core_->GetBounds();
  else
    return gfx::Rect();
}

void WebContentsViewAndroid::ShowContextMenu(
    const ContextMenuParams& params,
    const content::ContextMenuSourceType& type) {
  if (delegate_.get())
    delegate_->ShowContextMenu(params, type);
}

void WebContentsViewAndroid::ShowPopupMenu(
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    const std::vector<WebMenuItem>& items,
    bool right_aligned,
    bool allow_multiple_selection) {
  if (content_view_core_) {
    content_view_core_->ShowSelectPopupMenu(
        items, selected_item, allow_multiple_selection);
  }
}

void WebContentsViewAndroid::StartDragging(
    const WebDropData& drop_data,
    WebKit::WebDragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Point& image_offset) {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::UpdateDragCursor(WebKit::WebDragOperation op) {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::GotFocus() {
  // This is only used in the views FocusManager stuff but it bleeds through
  // all subclasses. http://crbug.com/21875
}

// This is called when we the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void WebContentsViewAndroid::TakeFocus(bool reverse) {
  if (web_contents_->GetDelegate() &&
      web_contents_->GetDelegate()->TakeFocus(web_contents_, reverse))
    return;
  web_contents_->GetRenderWidgetHostView()->Focus();
}

} // namespace content
