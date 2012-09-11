// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/draw_delegate_impl.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/android/device_info.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"

namespace content {

RenderWidgetHostViewAndroid::RenderWidgetHostViewAndroid(
    RenderWidgetHostImpl* widget_host,
    ContentViewCoreImpl* content_view_core)
    : host_(widget_host),
      // ContentViewCoreImpl represents the native side of the Java
      // ContentViewCore.  It being NULL means that it is not attached to the
      // View system yet, so we treat it as hidden.
      is_hidden_(!content_view_core),
      content_view_core_(content_view_core),
      ime_adapter_android_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      texture_layer_(WebKit::WebExternalTextureLayer::create()) {
  host_->SetView(this);
  // RenderWidgetHost is initialized as visible. If is_hidden_ is true, tell
  // RenderWidgetHost to hide.
  if (is_hidden_)
    host_->WasHidden();
  texture_layer_->layer()->setDrawsContent(!is_hidden_);
  host_->AttachLayer(texture_layer_->layer());
}

RenderWidgetHostViewAndroid::~RenderWidgetHostViewAndroid() {
}

void RenderWidgetHostViewAndroid::InitAsChild(gfx::NativeView parent_view) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTIMPLEMENTED();
}

RenderWidgetHost*
RenderWidgetHostViewAndroid::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewAndroid::WasShown() {
  if (!is_hidden_)
    return;
  is_hidden_ = false;
  host_->WasShown();
}

void RenderWidgetHostViewAndroid::WasHidden() {
  if (is_hidden_)
    return;

  // If we receive any more paint messages while we are hidden, we want to
  // ignore them so we don't re-allocate the backing store.  We will paint
  // everything again when we become visible again.
  //
  is_hidden_ = true;

  // Inform the renderer that we are being hidden so it can reduce its resource
  // utilization.
  host_->WasHidden();
}

void RenderWidgetHostViewAndroid::SetSize(const gfx::Size& size) {
  // Update the size of the RWH.
  if (requested_size_.width() != size.width() ||
      requested_size_.height() != size.height()) {
    requested_size_ = gfx::Size(size.width(), size.height());
    host_->WasResized();
  }
  texture_layer_->layer()->setBounds(size);
}

void RenderWidgetHostViewAndroid::SetBounds(const gfx::Rect& rect) {
  if (rect.origin().x() || rect.origin().y()) {
    VLOG(0) << "SetBounds not implemented for (x,y)!=(0,0)";
  }
  SetSize(rect.size());
}

gfx::NativeView RenderWidgetHostViewAndroid::GetNativeView() const {
  return content_view_core_;
}

gfx::NativeViewId RenderWidgetHostViewAndroid::GetNativeViewId() const {
  return reinterpret_cast<gfx::NativeViewId>(
      const_cast<RenderWidgetHostViewAndroid*>(this));
}

gfx::NativeViewAccessible
RenderWidgetHostViewAndroid::GetNativeViewAccessible() {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewAndroid::MovePluginWindows(
    const gfx::Point& scroll_offset,
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
  // We don't have plugin windows on Android. Do nothing. Note: this is called
  // from RenderWidgetHost::OnMsgUpdateRect which is itself invoked while
  // processing the corresponding message from Renderer.
}

void RenderWidgetHostViewAndroid::Focus() {
  host_->Focus();
  host_->SetInputMethodActive(true);
}

void RenderWidgetHostViewAndroid::Blur() {
  host_->Send(new ViewMsg_ExecuteEditCommand(
      host_->GetRoutingID(), "Unselect", ""));
  host_->SetInputMethodActive(false);
  host_->Blur();
}

bool RenderWidgetHostViewAndroid::HasFocus() const {
  if (!content_view_core_)
    return false;  // ContentViewCore not created yet.

  return content_view_core_->HasFocus();
}

bool RenderWidgetHostViewAndroid::IsSurfaceAvailableForCopy() const {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewAndroid::Show() {
  texture_layer_->layer()->setDrawsContent(true);
}

void RenderWidgetHostViewAndroid::Hide() {
  texture_layer_->layer()->setDrawsContent(false);
}

bool RenderWidgetHostViewAndroid::IsShowing() {
  return !is_hidden_;
}

gfx::Rect RenderWidgetHostViewAndroid::GetViewBounds() const {
  gfx::Size bounds = DrawDelegateImpl::GetInstance()->GetBounds();
  if (!bounds.IsEmpty())
    return gfx::Rect(bounds);

  if (content_view_core_) {
    return content_view_core_->GetBounds();
  } else {
    // The ContentViewCore has not been created yet. This only happens when
    // renderer asks for creating new window, for example,
    // javascript window.open().
    return gfx::Rect(0, 0, 0, 0);
  }
}

void RenderWidgetHostViewAndroid::UpdateCursor(const WebCursor& cursor) {
  // There are no cursors on Android.
}

void RenderWidgetHostViewAndroid::SetIsLoading(bool is_loading) {
  // Do nothing. The UI notification is handled through ContentViewClient which
  // is TabContentsDelegate.
}

void RenderWidgetHostViewAndroid::TextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  if (is_hidden_)
    return;

  content_view_core_->ImeUpdateAdapter(
      GetNativeImeAdapter(),
      static_cast<int>(params.type),
      params.value, params.selection_start, params.selection_end,
      params.composition_start, params.composition_end,
      false /* show_ime_if_needed */);
}

int RenderWidgetHostViewAndroid::GetNativeImeAdapter() {
  return reinterpret_cast<int>(&ime_adapter_android_);
}

void RenderWidgetHostViewAndroid::ImeCancelComposition() {
  ime_adapter_android_.CancelComposition();
}

void RenderWidgetHostViewAndroid::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
    const std::vector<gfx::Rect>& copy_rects) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::RenderViewGone(
    base::TerminationStatus status, int error_code) {
  Destroy();
}

void RenderWidgetHostViewAndroid::Destroy() {
  host_->RemoveLayer(texture_layer_->layer());

  content_view_core_ = NULL;

  // The RenderWidgetHost's destruction led here, so don't call it.
  host_ = NULL;

  delete this;
}

void RenderWidgetHostViewAndroid::SetTooltipText(
    const string16& tooltip_text) {
  // Tooltips don't makes sense on Android.
}

void RenderWidgetHostViewAndroid::SelectionChanged(const string16& text,
                                                   size_t offset,
                                                   const ui::Range& range) {
  RenderWidgetHostViewBase::SelectionChanged(text, offset, range);

  if (text.empty() || range.is_empty() || !content_view_core_)
    return;
  size_t pos = range.GetMin() - offset;
  size_t n = range.length();

  DCHECK(pos + n <= text.length()) << "The text can not fully cover range.";
  if (pos >= text.length()) {
    NOTREACHED() << "The text can not cover range.";
    return;
  }

  std::string utf8_selection = UTF16ToUTF8(text.substr(pos, n));

  content_view_core_->OnSelectionChanged(utf8_selection);
}

BackingStore* RenderWidgetHostViewAndroid::AllocBackingStore(
    const gfx::Size& size) {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewAndroid::SetBackground(const SkBitmap& background) {
  RenderWidgetHostViewBase::SetBackground(background);
  host_->Send(new ViewMsg_SetBackground(host_->GetRoutingID(), background));
}

void RenderWidgetHostViewAndroid::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool)>& callback,
    skia::PlatformCanvas* output) {
  NOTIMPLEMENTED();
  callback.Run(false);
}

void RenderWidgetHostViewAndroid::OnAcceleratedCompositingStateChange() {
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
  texture_layer_->setTextureId(params.surface_handle);
  texture_layer_->layer()->invalidate();
  // TODO(sievers): The view and layer should get sized proactively.
  if (((gfx::Size)texture_layer_->layer()->bounds()).IsEmpty())
    texture_layer_->layer()->setBounds(
        DrawDelegateImpl::GetInstance()->GetBounds());
  DrawDelegateImpl::GetInstance()->OnSurfaceUpdated(
      params.surface_handle,
      this,
      base::Bind(&RenderWidgetHostImpl::AcknowledgeBufferPresent,
                 params.route_id, gpu_host_id));
}

void RenderWidgetHostViewAndroid::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
    int gpu_host_id) {
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceSuspend() {
  NOTREACHED();
}

bool RenderWidgetHostViewAndroid::HasAcceleratedSurface(
    const gfx::Size& desired_size) {
  NOTREACHED();
  return false;
}

void RenderWidgetHostViewAndroid::StartContentIntent(
    const GURL& content_url) {
  if (content_view_core_)
    content_view_core_->StartContentIntent(content_url);
}

gfx::GLSurfaceHandle RenderWidgetHostViewAndroid::GetCompositingSurface() {
  gfx::GLSurfaceHandle handle =
      DrawDelegateImpl::GetInstance()->GetDrawSurface();
  if (!handle.is_null())
    return handle;

  // On Android, we cannot generate a window handle that can be passed to the
  // GPU process through the native side. Instead, we send the surface handle
  // through Binder after the compositing context has been created.
  return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, true);
}

void RenderWidgetHostViewAndroid::GetScreenInfo(WebKit::WebScreenInfo* result) {
  // ScreenInfo isn't tied to the widget on Android. Always return the default.
  RenderWidgetHostViewBase::GetDefaultScreenInfo(result);
}

// TODO(jrg): Find out the implications and answer correctly here,
// as we are returning the WebView and not root window bounds.
gfx::Rect RenderWidgetHostViewAndroid::GetBoundsInRootWindow() {
  return GetViewBounds();
}

void RenderWidgetHostViewAndroid::UnhandledWheelEvent(
    const WebKit::WebMouseWheelEvent& event) {
  // intentionally empty, like RenderWidgetHostViewViews
}

void RenderWidgetHostViewAndroid::ProcessTouchAck(
    WebKit::WebInputEvent::Type type, bool processed) {
  // intentionally empty, like RenderWidgetHostViewViews
}

void RenderWidgetHostViewAndroid::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
  // intentionally empty, like RenderWidgetHostViewViews
}

void RenderWidgetHostViewAndroid::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  // intentionally empty, like RenderWidgetHostViewViews
}

bool RenderWidgetHostViewAndroid::LockMouse() {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewAndroid::UnlockMouse() {
  NOTIMPLEMENTED();
}

// Methods called from the host to the render

void RenderWidgetHostViewAndroid::SendKeyEvent(
    const NativeWebKeyboardEvent& event) {
  if (host_)
    host_->ForwardKeyboardEvent(event);
}

void RenderWidgetHostViewAndroid::TouchEvent(
    const WebKit::WebTouchEvent& event) {
  if (host_)
    host_->ForwardTouchEvent(event);
}

void RenderWidgetHostViewAndroid::GestureEvent(
    const WebKit::WebGestureEvent& event) {
  if (host_)
    host_->ForwardGestureEvent(event);
}

void RenderWidgetHostViewAndroid::SelectRange(const gfx::Point& start,
                                              const gfx::Point& end) {
  if (host_)
    host_->SelectRange(start, end);
}

void RenderWidgetHostViewAndroid::SetContentViewCore(
    ContentViewCoreImpl* content_view_core) {
  content_view_core_ = content_view_core;
  if (host_) {
    GpuSurfaceTracker::Get()->SetSurfaceHandle(
        host_->surface_id(), content_view_core_ ?
            GetCompositingSurface() : gfx::GLSurfaceHandle());
  }
}

void RenderWidgetHostViewAndroid::DidSetNeedTouchEvents(
    bool need_touch_events) {
  if (content_view_core_)
    content_view_core_->DidSetNeedTouchEvents(need_touch_events);
}

// static
void RenderWidgetHostViewPort::GetDefaultScreenInfo(
    ::WebKit::WebScreenInfo* results) {
  DeviceInfo info;
  const int width = info.GetWidth();
  const int height = info.GetHeight();
  results->horizontalDPI = 160 * info.GetDPIScale();
  results->verticalDPI = 160 * info.GetDPIScale();
  results->depth = info.GetBitsPerPixel();
  results->depthPerComponent = info.GetBitsPerComponent();
  results->isMonochrome = (results->depthPerComponent == 0);
  results->rect = ::WebKit::WebRect(0, 0, width, height);
  // TODO(husky): Remove any system controls from availableRect.
  results->availableRect = ::WebKit::WebRect(0, 0, width, height);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostView, public:

// static
RenderWidgetHostView*
RenderWidgetHostView::CreateViewForWidget(RenderWidgetHost* widget) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(widget);
  return new RenderWidgetHostViewAndroid(rwhi, NULL);
}

} // namespace content
