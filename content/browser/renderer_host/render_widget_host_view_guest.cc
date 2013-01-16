// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_guest.h"
#include "content/common/browser_plugin_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "webkit/plugins/npapi/webplugin.h"

namespace content {

RenderWidgetHostViewGuest::RenderWidgetHostViewGuest(
    RenderWidgetHost* widget_host,
    BrowserPluginGuest* guest)
    : host_(RenderWidgetHostImpl::From(widget_host)),
      is_hidden_(false),
      guest_(guest) {
  host_->SetView(this);
}

RenderWidgetHostViewGuest::~RenderWidgetHostViewGuest() {
}

RenderWidgetHost* RenderWidgetHostViewGuest::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewGuest::WasShown() {
  if (!is_hidden_)
    return;

  is_hidden_ = false;
  host_->WasShown();
}

void RenderWidgetHostViewGuest::WasHidden() {
  if (is_hidden_)
    return;

  is_hidden_ = true;
  host_->WasHidden();
}

void RenderWidgetHostViewGuest::SetSize(const gfx::Size& size) {
  size_ = size;
  host_->WasResized();
}

gfx::Rect RenderWidgetHostViewGuest::GetBoundsInRootWindow() {
  return GetViewBounds();
}

gfx::GLSurfaceHandle RenderWidgetHostViewGuest::GetCompositingSurface() {
  return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, true);
}

void RenderWidgetHostViewGuest::Show() {
  WasShown();
}

void RenderWidgetHostViewGuest::Hide() {
  WasHidden();
}

bool RenderWidgetHostViewGuest::IsShowing() {
  return !is_hidden_;
}

gfx::Rect RenderWidgetHostViewGuest::GetViewBounds() const {
  return gfx::Rect(0, 0, size_.width(), size_.height());
}

void RenderWidgetHostViewGuest::RenderViewGone(base::TerminationStatus status,
                                               int error_code) {
  Destroy();
}

void RenderWidgetHostViewGuest::Destroy() {
  // The RenderWidgetHost's destruction led here, so don't call it.
  host_ = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void RenderWidgetHostViewGuest::SetTooltipText(const string16& tooltip_text) {
}

void RenderWidgetHostViewGuest::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
  guest_->SendMessageToEmbedder(
      new BrowserPluginMsg_BuffersSwapped(
          guest_->embedder_routing_id(),
          guest_->instance_id(),
          params.size,
          params.mailbox_name,
          params.route_id,
          gpu_host_id));
}

void RenderWidgetHostViewGuest::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
    int gpu_host_id) {
  guest_->SendMessageToEmbedder(
      new BrowserPluginMsg_BuffersSwapped(
          guest_->embedder_routing_id(),
          guest_->instance_id(),
          params.surface_size,
          params.mailbox_name,
          params.route_id,
          gpu_host_id));
}

void RenderWidgetHostViewGuest::SetBounds(const gfx::Rect& rect) {
  SetSize(rect.size());
}

void RenderWidgetHostViewGuest::InitAsChild(
    gfx::NativeView parent_view) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTIMPLEMENTED();
}

gfx::NativeView RenderWidgetHostViewGuest::GetNativeView() const {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::NativeViewId RenderWidgetHostViewGuest::GetNativeViewId() const {
  NOTIMPLEMENTED();
  return static_cast<gfx::NativeViewId>(NULL);
}

gfx::NativeViewAccessible RenderWidgetHostViewGuest::GetNativeViewAccessible() {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewGuest::MovePluginWindows(
    const gfx::Vector2d& scroll_offset,
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::Focus() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::Blur() {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewGuest::HasFocus() const {
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewGuest::IsSurfaceAvailableForCopy() const {
  NOTIMPLEMENTED();
  return true;
}

void RenderWidgetHostViewGuest::UpdateCursor(const WebCursor& cursor) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::SetIsLoading(bool is_loading) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::TextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::ImeCancelComposition() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect,
    const gfx::Vector2d& scroll_delta,
    const std::vector<gfx::Rect>& copy_rects) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::SelectionBoundsChanged(
    const gfx::Rect& start_rect,
    WebKit::WebTextDirection start_direction,
    const gfx::Rect& end_rect,
    WebKit::WebTextDirection end_direction) {
  NOTIMPLEMENTED();
}

BackingStore* RenderWidgetHostViewGuest::AllocBackingStore(
    const gfx::Size& size) {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewGuest::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& /* dst_size */,
    const base::Callback<void(bool)>& callback,
    skia::PlatformBitmap* output) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::AcceleratedSurfaceSuspend() {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewGuest::HasAcceleratedSurface(
      const gfx::Size& desired_size) {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewGuest::SetBackground(const SkBitmap& background) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::OnAcceleratedCompositingStateChange() {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewGuest::LockMouse() {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewGuest::UnlockMouse() {
  NOTIMPLEMENTED();
}

#if defined(OS_MACOSX)
void RenderWidgetHostViewGuest::SetActive(bool active) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::SetTakesFocusOnlyOnMouseDown(bool flag) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::SetWindowVisibility(bool visible) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::WindowFrameChanged() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::ShowDefinitionForSelection() {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewGuest::SupportsSpeech() const {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewGuest::SpeakSelection() {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewGuest::IsSpeaking() const {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewGuest::StopSpeaking() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::AboutToWaitForBackingStoreMsg() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::PluginFocusChanged(bool focused,
                                                   int plugin_id) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::StartPluginIme() {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewGuest::PostProcessEventForPluginIme(
    const NativeWebKeyboardEvent& event) {
  NOTIMPLEMENTED();
  return false;
}

gfx::PluginWindowHandle
RenderWidgetHostViewGuest::AllocateFakePluginWindowHandle(
    bool opaque, bool root) {
  NOTIMPLEMENTED();
  return 0;
}

void RenderWidgetHostViewGuest::DestroyFakePluginWindowHandle(
    gfx::PluginWindowHandle window) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::AcceleratedSurfaceSetIOSurface(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    uint64 io_surface_identifier) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::AcceleratedSurfaceSetTransportDIB(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    TransportDIB::Handle transport_dib) {
  NOTIMPLEMENTED();
}
#endif  // defined(OS_MACOSX)

#if defined(OS_ANDROID)
void RenderWidgetHostViewGuest::StartContentIntent(const GURL& content_url) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::SetCachedBackgroundColor(SkColor color) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::ShowDisambiguationPopup(
    const gfx::Rect& target_rect,
    const SkBitmap& zoomed_bitmap) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::SetCachedPageScaleFactorLimits(
    float minimum_scale,
    float maximum_scale) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::UpdateFrameInfo(
    const gfx::Vector2d& scroll_offset,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor,
    const gfx::Size& content_size,
    const gfx::Vector2dF& controls_offset,
    const gfx::Vector2dF& content_offset) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::HasTouchEventHandlers(bool need_touch_events) {
  NOTIMPLEMENTED();
}
#endif  // defined(OS_ANDROID)

#if defined(TOOLKIT_GTK)
GdkEventButton* RenderWidgetHostViewGuest::GetLastMouseDown() {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::NativeView RenderWidgetHostViewGuest::BuildInputMethodsGtkMenu() {
  NOTIMPLEMENTED();
  return gfx::NativeView();
}

void RenderWidgetHostViewGuest::CreatePluginContainer(
    gfx::PluginWindowHandle id) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::DestroyPluginContainer(
    gfx::PluginWindowHandle id) {
  NOTIMPLEMENTED();
}
#endif  // defined(TOOLKIT_GTK)

#if defined(OS_WIN) && !defined(USE_AURA)
void RenderWidgetHostViewGuest::WillWmDestroy() {
  NOTIMPLEMENTED();
}
#endif

#if defined(OS_POSIX) || defined(USE_AURA)
void RenderWidgetHostViewGuest::GetScreenInfo(WebKit::WebScreenInfo* results) {
  NOTIMPLEMENTED();
}
#endif  // defined(OS_POSIX) || defined(USE_AURA)

}  // namespace content
