// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_render_widget_host_view.h"

#include "base/logging.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "content/browser/renderer_host/render_widget_host.h"

namespace prerender {

PrerenderRenderWidgetHostView::PrerenderRenderWidgetHostView(
    RenderWidgetHost* render_widget_host,
    PrerenderContents* prerender_contents)
    : render_widget_host_(render_widget_host),
      prerender_contents_(prerender_contents) {
  render_widget_host_->set_view(this);
}

PrerenderRenderWidgetHostView::~PrerenderRenderWidgetHostView() {
}

void PrerenderRenderWidgetHostView::Init(RenderWidgetHostView* view) {
#if defined(OS_MACOSX)
  cocoa_view_bounds_ = view->GetViewBounds();
  root_window_rect_ = view->GetRootWindowRect();
#endif  // defined(OS_MACOSX)
  SetBounds(view->GetViewBounds());

  // PrerenderRenderWidgetHostViews are always hidden.  This reduces the
  // process priority of the render process while prerendering, and prevents it
  // from painting anything until the page is actually displayed.
  WasHidden();
}

void PrerenderRenderWidgetHostView::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::InitAsFullscreen() {
  NOTREACHED();
}

RenderWidgetHost* PrerenderRenderWidgetHostView::GetRenderWidgetHost() const {
  return render_widget_host_;
}

void PrerenderRenderWidgetHostView::DidBecomeSelected() {
  // The View won't be shown during prerendering.
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::WasHidden() {
  render_widget_host_->WasHidden();
}

void PrerenderRenderWidgetHostView::SetSize(const gfx::Size& size) {
  SetBounds(gfx::Rect(GetViewBounds().origin(), size));
}

void PrerenderRenderWidgetHostView::SetBounds(const gfx::Rect& rect) {
  bounds_ = rect;
  if (render_widget_host_)
    render_widget_host_->WasResized();
}

gfx::NativeView PrerenderRenderWidgetHostView::GetNativeView() {
  return NULL;
}

void PrerenderRenderWidgetHostView::MovePluginWindows(
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
  // Since a PrerenderRenderWidgetHostView is always hidden and plugins are not
  // loaded during prerendering, this will never be called.
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::Focus() {
  // A PrerenderRenderWidgetHostView can never be focused, as it's always
  // hidden.
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::Blur() {
}

bool PrerenderRenderWidgetHostView::HasFocus() {
  return false;
}

void PrerenderRenderWidgetHostView::Show() {
  // A PrerenderRenderWidgetHostView can never be shown, as it is not yet
  // associated with a tab.
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::Hide() {
  WasHidden();
}

bool PrerenderRenderWidgetHostView::IsShowing() {
  return false;
}

gfx::Rect PrerenderRenderWidgetHostView::GetViewBounds() const {
  return bounds_;
}

void PrerenderRenderWidgetHostView::UpdateCursor(const WebCursor& cursor) {
  // The cursor should only be updated in response to a mouse event, which
  // hidden RenderViews don't have.
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::SetIsLoading(bool is_loading) {
  // Do nothing.  PrerenderContents manages this flag.
}

void PrerenderRenderWidgetHostView::ImeUpdateTextInputState(
    WebKit::WebTextInputType type,
    const gfx::Rect& caret_rect) {
  // Not called on hidden views.
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::ImeCancelComposition() {
  // Not called on hidden views.
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
    const std::vector<gfx::Rect>& copy_rects) {
  // Since prerendering RenderViewsHosts are always hidden, this will not be
  // be called.
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::RenderViewGone(
    base::TerminationStatus status, int error_code) {
  // TODO(mmenke): This should result in the PrerenderContents cancelling the
  //               prerender itself.
  Destroy();
}

void PrerenderRenderWidgetHostView::WillDestroyRenderWidget(
    RenderWidgetHost* rwh) {
  if (rwh == render_widget_host_)
    render_widget_host_ = NULL;
}

void PrerenderRenderWidgetHostView::Destroy() {
  delete this;
}

void PrerenderRenderWidgetHostView::SetTooltipText(
    const std::wstring& tooltip_text) {
  // Since this is only set on mouse move and the View can't be focused, this
  // will never be called.
  NOTREACHED();
}

BackingStore* PrerenderRenderWidgetHostView::AllocBackingStore(
    const gfx::Size& size) {
  // Since prerendering RenderViewsHosts are always hidden, this will not be
  // be called.
  NOTREACHED();
  return NULL;
}

#if defined(OS_MACOSX)
void PrerenderRenderWidgetHostView::SetTakesFocusOnlyOnMouseDown(bool flag) {
  // This is only used by on RenderWidgetHosts currently in a TabContents.
  NOTREACHED();
}

gfx::Rect PrerenderRenderWidgetHostView::GetViewCocoaBounds() const {
  return cocoa_view_bounds_;
}

gfx::Rect PrerenderRenderWidgetHostView::GetRootWindowRect() {
  return root_window_rect_;
}

void PrerenderRenderWidgetHostView::SetActive(bool active) {
  // This view can never be made visible.
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::SetWindowVisibility(bool visible) {
  // This view can never be made visible.
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::WindowFrameChanged() {
  // Since there's no frame, and nothing to notify |this| of its change, this
  // will never get called.
  NOTREACHED();
}

// Since plugins are not loaded until the prerendered page is displayed, none of
// the following functions will be called.
void PrerenderRenderWidgetHostView::PluginFocusChanged(bool focused,
                                                       int plugin_id) {
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::StartPluginIme() {
  NOTREACHED();
}

bool PrerenderRenderWidgetHostView::PostProcessEventForPluginIme(
    const NativeWebKeyboardEvent& event) {
  NOTREACHED();
  return false;
}

gfx::PluginWindowHandle
PrerenderRenderWidgetHostView::AllocateFakePluginWindowHandle(
    bool opaque, bool root) {
  NOTREACHED();
  return gfx::kNullPluginWindow;
}

void PrerenderRenderWidgetHostView::DestroyFakePluginWindowHandle(
    gfx::PluginWindowHandle window) {
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::AcceleratedSurfaceSetIOSurface(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    uint64 io_surface_identifier) {
}

void PrerenderRenderWidgetHostView::AcceleratedSurfaceSetTransportDIB(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    TransportDIB::Handle transport_dib) {
}

void PrerenderRenderWidgetHostView::AcceleratedSurfaceBuffersSwapped(
    gfx::PluginWindowHandle window,
    uint64 surface_id,
    int renderer_id,
    int32 route_id,
    int gpu_host_id,
    uint64 swap_buffers_count) {
}

void PrerenderRenderWidgetHostView::GpuRenderingStateDidChange() {
}
#endif  // defined(OS_MACOSX)

#if defined(TOOLKIT_USES_GTK)
void PrerenderRenderWidgetHostView::CreatePluginContainer(
    gfx::PluginWindowHandle id) {
  // Since plugins are not loaded until the prerendered page is loaded, this
  // should never be called.
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::DestroyPluginContainer(
    gfx::PluginWindowHandle id) {
}

void PrerenderRenderWidgetHostView::AcceleratedCompositingActivated(
    bool activated) {
}
#endif  // defined(TOOLKIT_USES_GTK)

#if defined(OS_WIN)
void PrerenderRenderWidgetHostView::WillWmDestroy() {
}

void PrerenderRenderWidgetHostView::ShowCompositorHostWindow(bool show) {
}
#endif  // defined(OS_WIN)

gfx::PluginWindowHandle
PrerenderRenderWidgetHostView::GetCompositingSurface() {
  return gfx::kNullPluginWindow;
}

void PrerenderRenderWidgetHostView::SetVisuallyDeemphasized(
    const SkColor* color, bool animate) {
  // This will not be called until the RVH is swapped into a tab.
  NOTREACHED();
}

void PrerenderRenderWidgetHostView::SetBackground(const SkBitmap& background) {
  // This will not be called for HTTP RenderViews.
  NOTREACHED();
}

bool PrerenderRenderWidgetHostView::ContainsNativeView(
    gfx::NativeView native_view) const {
  return false;
}

}  // namespace prerender
