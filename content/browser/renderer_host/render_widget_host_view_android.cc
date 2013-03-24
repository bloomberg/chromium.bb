// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include <android/bitmap.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "cc/layers/layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/image_transport_factory_android.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/surface_texture_transport_client_android.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebExternalTextureLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"
#include "webkit/compositor_bindings/web_compositor_support_impl.h"

namespace content {

namespace {

void InsertSyncPointAndAckForGpu(
    int gpu_host_id, int route_id, const std::string& return_mailbox) {
  uint32 sync_point =
      ImageTransportFactoryAndroid::GetInstance()->InsertSyncPoint();
  AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
  ack_params.mailbox_name = return_mailbox;
  ack_params.sync_point = sync_point;
  RenderWidgetHostImpl::AcknowledgeBufferPresent(
      route_id, gpu_host_id, ack_params);
}

void InsertSyncPointAndAckForCompositor(
    int renderer_host_id,
    int route_id,
    const gpu::Mailbox& return_mailbox,
    const gfx::Size return_size) {
  cc::CompositorFrameAck ack;
  ack.gl_frame_data.reset(new cc::GLFrameData());
  if (!return_mailbox.IsZero()) {
    ack.gl_frame_data->mailbox = return_mailbox;
    ack.gl_frame_data->size = return_size;
    ack.gl_frame_data->sync_point =
        ImageTransportFactoryAndroid::GetInstance()->InsertSyncPoint();
  }
  RenderWidgetHostImpl::SendSwapCompositorFrameAck(
      route_id, renderer_host_id, ack);
}

}  // anonymous namespace

RenderWidgetHostViewAndroid::RenderWidgetHostViewAndroid(
    RenderWidgetHostImpl* widget_host,
    ContentViewCoreImpl* content_view_core)
    : host_(widget_host),
      is_layer_attached_(true),
      content_view_core_(NULL),
      ime_adapter_android_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      cached_background_color_(SK_ColorWHITE),
      texture_id_in_layer_(0) {
  if (CompositorImpl::UsesDirectGL()) {
    surface_texture_transport_.reset(new SurfaceTextureTransportClient());
    layer_ = surface_texture_transport_->Initialize();
  } else {
    texture_layer_ = cc::TextureLayer::Create(NULL);
    layer_ = texture_layer_;
  }

  layer_->SetContentsOpaque(true);
  layer_->SetIsDrawable(true);

  host_->SetView(this);
  SetContentViewCore(content_view_core);
}

RenderWidgetHostViewAndroid::~RenderWidgetHostViewAndroid() {
  SetContentViewCore(NULL);
  if (texture_id_in_layer_) {
    ImageTransportFactoryAndroid::GetInstance()->DeleteTexture(
        texture_id_in_layer_);
  }
}


bool RenderWidgetHostViewAndroid::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetHostViewAndroid, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ImeBatchStateChanged_ACK,
                        OnProcessImeBatchStateAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartContentIntent, OnStartContentIntent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidChangeBodyBackgroundColor,
                        OnDidChangeBodyBackgroundColor)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
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
  if (!host_->is_hidden())
    return;

  host_->WasShown();
}

void RenderWidgetHostViewAndroid::WasHidden() {
  if (host_->is_hidden())
    return;

  // Inform the renderer that we are being hidden so it can reduce its resource
  // utilization.
  host_->WasHidden();
}

void RenderWidgetHostViewAndroid::WasResized() {
  if (surface_texture_transport_.get() && content_view_core_)
    surface_texture_transport_->SetSize(
        content_view_core_->GetPhysicalBackingSize());

  host_->WasResized();
}

void RenderWidgetHostViewAndroid::SetSize(const gfx::Size& size) {
  // Ignore the given size as only the Java code has the power to
  // resize the view on Android.
  WasResized();
}

void RenderWidgetHostViewAndroid::SetBounds(const gfx::Rect& rect) {
  SetSize(rect.size());
}

WebKit::WebGLId RenderWidgetHostViewAndroid::GetScaledContentTexture(
    float scale,
    gfx::Size* out_size) {
  gfx::Size size(gfx::ToCeiledSize(
      gfx::ScaleSize(texture_size_in_layer_, scale)));

  if (!CompositorImpl::IsInitialized() ||
      texture_id_in_layer_ == 0 ||
      texture_size_in_layer_.IsEmpty() ||
      size.IsEmpty()) {
    if (out_size)
        out_size->SetSize(0, 0);

    return 0;
  }

  if (out_size)
    *out_size = size;

  GLHelper* helper = ImageTransportFactoryAndroid::GetInstance()->GetGLHelper();
  return helper->CopyAndScaleTexture(texture_id_in_layer_,
                                     texture_size_in_layer_,
                                     size,
                                     true);
}

bool RenderWidgetHostViewAndroid::PopulateBitmapWithContents(jobject jbitmap) {
  if (!CompositorImpl::IsInitialized() ||
      texture_id_in_layer_ == 0 ||
      texture_size_in_layer_.IsEmpty())
    return false;

  gfx::JavaBitmap bitmap(jbitmap);

  // TODO(dtrainor): Eventually add support for multiple formats here.
  DCHECK(bitmap.format() == ANDROID_BITMAP_FORMAT_RGBA_8888);

  GLHelper* helper = ImageTransportFactoryAndroid::GetInstance()->GetGLHelper();

  WebKit::WebGLId texture = helper->CopyAndScaleTexture(texture_id_in_layer_,
                                                        texture_size_in_layer_,
                                                        bitmap.size(),
                                                        true);
  if (texture == 0)
    return false;

  helper->ReadbackTextureSync(texture,
                              gfx::Rect(bitmap.size()),
                              static_cast<unsigned char*> (bitmap.pixels()));

  WebKit::WebGraphicsContext3D* context =
      ImageTransportFactoryAndroid::GetInstance()->GetContext3D();
  context->deleteTexture(texture);

  return true;
}

bool RenderWidgetHostViewAndroid::HasValidFrame() const {
  return texture_id_in_layer_ != 0 &&
      content_view_core_ &&
      !texture_size_in_layer_.IsEmpty();
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
    const gfx::Vector2d& scroll_offset,
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
  // We don't have plugin windows on Android. Do nothing. Note: this is called
  // from RenderWidgetHost::OnUpdateRect which is itself invoked while
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
  if (is_layer_attached_)
    return;

  is_layer_attached_ = true;
  if (content_view_core_)
    content_view_core_->AttachLayer(layer_);
}

void RenderWidgetHostViewAndroid::Hide() {
  if (!is_layer_attached_)
    return;

  is_layer_attached_ = false;
  if (content_view_core_)
    content_view_core_->RemoveLayer(layer_);
}

bool RenderWidgetHostViewAndroid::IsShowing() {
  // ContentViewCoreImpl represents the native side of the Java
  // ContentViewCore.  It being NULL means that it is not attached
  // to the View system yet, so we treat this RWHVA as hidden.
  return is_layer_attached_ && content_view_core_;
}

gfx::Rect RenderWidgetHostViewAndroid::GetViewBounds() const {
  if (!content_view_core_)
    return gfx::Rect();

  // If the backing hasn't been initialized yet, report empty view bounds
  // as well. Otherwise, we may end up stuck in a white-screen state because
  // the resize ack is sent after swapbuffers.
  if (GetPhysicalBackingSize().IsEmpty())
    return gfx::Rect();

  return gfx::Rect(content_view_core_->GetViewportSizeDip());
}

gfx::Size RenderWidgetHostViewAndroid::GetPhysicalBackingSize() const {
  if (!content_view_core_)
    return gfx::Size();

  return content_view_core_->GetPhysicalBackingSize();
}

float RenderWidgetHostViewAndroid::GetOverdrawBottomHeight() const {
  if (!content_view_core_)
    return 0.f;

  return content_view_core_->GetOverdrawBottomHeightDip();
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
  if (!IsShowing())
    return;

  content_view_core_->UpdateImeAdapter(
      GetNativeImeAdapter(),
      static_cast<int>(params.type),
      params.value, params.selection_start, params.selection_end,
      params.composition_start, params.composition_end,
      params.show_ime_if_needed);
}

int RenderWidgetHostViewAndroid::GetNativeImeAdapter() {
  return reinterpret_cast<int>(&ime_adapter_android_);
}

void RenderWidgetHostViewAndroid::OnProcessImeBatchStateAck(bool is_begin) {
  if (content_view_core_)
    content_view_core_->ProcessImeBatchStateAck(is_begin);
}

void RenderWidgetHostViewAndroid::OnDidChangeBodyBackgroundColor(
    SkColor color) {
  if (cached_background_color_ == color)
    return;

  cached_background_color_ = color;
  if (content_view_core_)
    content_view_core_->OnBackgroundColorChanged(color);
}

void RenderWidgetHostViewAndroid::OnStartContentIntent(
    const GURL& content_url) {
  if (content_view_core_)
    content_view_core_->StartContentIntent(content_url);
}

void RenderWidgetHostViewAndroid::ImeCancelComposition() {
  ime_adapter_android_.CancelComposition();
}

void RenderWidgetHostViewAndroid::ImeCompositionRangeChanged(
    const ui::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
}

void RenderWidgetHostViewAndroid::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect,
    const gfx::Vector2d& scroll_delta,
    const std::vector<gfx::Rect>& copy_rects) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::RenderViewGone(
    base::TerminationStatus status, int error_code) {
  Destroy();
}

void RenderWidgetHostViewAndroid::Destroy() {
  if (content_view_core_) {
    content_view_core_->RemoveLayer(layer_);
    content_view_core_ = NULL;
  }

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

void RenderWidgetHostViewAndroid::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  if (content_view_core_) {
    content_view_core_->OnSelectionBoundsChanged(params);
  }
}

void RenderWidgetHostViewAndroid::ScrollOffsetChanged() {
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
    const base::Callback<void(bool, const SkBitmap&)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(false, SkBitmap());
}

void RenderWidgetHostViewAndroid::CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(false);
}

bool RenderWidgetHostViewAndroid::CanCopyToVideoFrame() const {
  return false;
}

void RenderWidgetHostViewAndroid::ShowDisambiguationPopup(
    const gfx::Rect& target_rect, const SkBitmap& zoomed_bitmap) {
  if (!content_view_core_)
    return;

  content_view_core_->ShowDisambiguationPopup(target_rect, zoomed_bitmap);
}

void RenderWidgetHostViewAndroid::OnAcceleratedCompositingStateChange() {
}

void RenderWidgetHostViewAndroid::OnSwapCompositorFrame(
    scoped_ptr<cc::CompositorFrame> frame) {
  if (!frame->gl_frame_data || frame->gl_frame_data->mailbox.IsZero())
    return;

  base::Closure callback = base::Bind(&InsertSyncPointAndAckForCompositor,
                                      host_->GetProcess()->GetID(),
                                      host_->GetRoutingID(),
                                      current_mailbox_,
                                      texture_size_in_layer_);
  ImageTransportFactoryAndroid::GetInstance()->WaitSyncPoint(
      frame->gl_frame_data->sync_point);
  const gfx::Size& texture_size = frame->gl_frame_data->size;

  // Calculate the content size.  This should be 0 if the texture_size is 0.
  float dp2px = frame->metadata.device_scale_factor;
  gfx::Vector2dF offset;
  if (texture_size.GetArea() > 0)
    offset = frame->metadata.location_bar_content_translation;
  gfx::SizeF content_size(texture_size.width() - offset.x() * dp2px,
                          texture_size.height() - offset.y() * dp2px);

  BuffersSwapped(frame->gl_frame_data->mailbox,
                 texture_size,
                 content_size,
                 callback);

}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
  NOTREACHED() << "Deprecated. Use --composite-to-mailbox.";

  if (params.mailbox_name.empty())
    return;

  std::string return_mailbox;
  if (!current_mailbox_.IsZero()) {
    return_mailbox.assign(
        reinterpret_cast<const char*>(current_mailbox_.name),
        sizeof(current_mailbox_.name));
  }

  base::Closure callback = base::Bind(&InsertSyncPointAndAckForGpu,
                                      gpu_host_id, params.route_id,
                                      return_mailbox);

  gpu::Mailbox mailbox;
  std::copy(params.mailbox_name.data(),
            params.mailbox_name.data() + params.mailbox_name.length(),
            reinterpret_cast<char*>(mailbox.name));

  BuffersSwapped(mailbox, params.size, params.size, callback);
}

void RenderWidgetHostViewAndroid::BuffersSwapped(
    const gpu::Mailbox& mailbox,
    const gfx::Size texture_size,
    const gfx::SizeF content_size,
    const base::Closure& ack_callback) {
  ImageTransportFactoryAndroid* factory =
      ImageTransportFactoryAndroid::GetInstance();

  // TODO(sievers): When running the impl thread in the browser we
  // need to delay the ACK until after commit and use more than a single
  // texture.
  DCHECK(!CompositorImpl::IsThreadingEnabled());

  if (texture_id_in_layer_) {
    DCHECK(!current_mailbox_.IsZero());
    ImageTransportFactoryAndroid::GetInstance()->ReleaseTexture(
        texture_id_in_layer_, current_mailbox_.name);
  } else {
    texture_id_in_layer_ = factory->CreateTexture();
    texture_layer_->SetTextureId(texture_id_in_layer_);
  }

  ImageTransportFactoryAndroid::GetInstance()->AcquireTexture(
      texture_id_in_layer_, mailbox.name);

  // We need to tell ContentViewCore about the new frame before calling
  // setNeedsDisplay() below so that it has the needed information schedule the
  // next compositor frame.
  if (content_view_core_)
    content_view_core_->DidProduceRendererFrame();

  texture_layer_->SetNeedsDisplay();
  texture_layer_->SetBounds(gfx::Size(content_size.width(),
                                      content_size.height()));

  // Calculate the uv_max based on the content size relative to the texture
  // size.
  gfx::PointF uv_max;
  if (texture_size.GetArea() > 0) {
    uv_max.SetPoint(content_size.width() / texture_size.width(),
                    content_size.height() / texture_size.height());
  }
  texture_layer_->SetUV(gfx::PointF(0, 0), uv_max);
  texture_size_in_layer_ = texture_size;
  current_mailbox_ = mailbox;
  ack_callback.Run();
}

void RenderWidgetHostViewAndroid::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
    int gpu_host_id) {
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceSuspend() {
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceRelease() {
  // This tells us we should free the frontbuffer.
  if (texture_id_in_layer_) {
    texture_layer_->SetTextureId(0);
    ImageTransportFactoryAndroid::GetInstance()->DeleteTexture(
        texture_id_in_layer_);
    texture_id_in_layer_ = 0;
    current_mailbox_ = gpu::Mailbox();
  }
}

bool RenderWidgetHostViewAndroid::HasAcceleratedSurface(
    const gfx::Size& desired_size) {
  NOTREACHED();
  return false;
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

gfx::GLSurfaceHandle RenderWidgetHostViewAndroid::GetCompositingSurface() {
  if (surface_texture_transport_.get()) {
    return surface_texture_transport_->GetCompositingSurface(
        host_->surface_id());
  } else {
    return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::TEXTURE_TRANSPORT);
  }
}

void RenderWidgetHostViewAndroid::ProcessAckedTouchEvent(
    const WebKit::WebTouchEvent& touch_event, InputEventAckState ack_result) {
  if (content_view_core_)
    content_view_core_->ConfirmTouchEvent(ack_result);
}

void RenderWidgetHostViewAndroid::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
  // intentionally empty, like RenderWidgetHostViewViews
}

void RenderWidgetHostViewAndroid::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  // intentionally empty, like RenderWidgetHostViewViews
}

void RenderWidgetHostViewAndroid::UnhandledWheelEvent(
    const WebKit::WebMouseWheelEvent& event) {
  // intentionally empty, like RenderWidgetHostViewViews
}

InputEventAckState RenderWidgetHostViewAndroid::FilterInputEvent(
    const WebKit::WebInputEvent& input_event) {
  if (!content_view_core_)
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;

  return content_view_core_->FilterInputEvent(input_event);
}

void RenderWidgetHostViewAndroid::OnAccessibilityNotifications(
    const std::vector<AccessibilityHostMsg_NotificationParams>& params) {
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

void RenderWidgetHostViewAndroid::SendTouchEvent(
    const WebKit::WebTouchEvent& event) {
  if (host_)
    host_->ForwardTouchEvent(event);
}


void RenderWidgetHostViewAndroid::SendMouseEvent(
    const WebKit::WebMouseEvent& event) {
  if (host_)
    host_->ForwardMouseEvent(event);
}

void RenderWidgetHostViewAndroid::SendMouseWheelEvent(
    const WebKit::WebMouseWheelEvent& event) {
  if (host_)
    host_->ForwardWheelEvent(event);
}

void RenderWidgetHostViewAndroid::SendGestureEvent(
    const WebKit::WebGestureEvent& event) {
  if (host_)
    host_->ForwardGestureEvent(event);
}

void RenderWidgetHostViewAndroid::SelectRange(const gfx::Point& start,
                                              const gfx::Point& end) {
  if (host_)
    host_->SelectRange(start, end);
}

void RenderWidgetHostViewAndroid::MoveCaret(const gfx::Point& point) {
  if (host_)
    host_->MoveCaret(point);
}

SkColor RenderWidgetHostViewAndroid::GetCachedBackgroundColor() const {
  return cached_background_color_;
}

void RenderWidgetHostViewAndroid::UpdateFrameInfo(
    const gfx::Vector2dF& scroll_offset,
    float page_scale_factor,
    const gfx::Vector2dF& page_scale_factor_limits,
    const gfx::SizeF& content_size,
    const gfx::SizeF& viewport_size,
    const gfx::Vector2dF& controls_offset,
    const gfx::Vector2dF& content_offset) {
  if (content_view_core_) {
    // All offsets and sizes are in CSS pixels.
    content_view_core_->UpdateFrameInfo(
        scroll_offset, page_scale_factor, page_scale_factor_limits,
        content_size, viewport_size, controls_offset, content_offset);
  }
}

void RenderWidgetHostViewAndroid::SetContentViewCore(
    ContentViewCoreImpl* content_view_core) {
  if (content_view_core_ && is_layer_attached_)
    content_view_core_->RemoveLayer(layer_);

  content_view_core_ = content_view_core;
  if (content_view_core_ && is_layer_attached_)
    content_view_core_->AttachLayer(layer_);
}

void RenderWidgetHostViewAndroid::HasTouchEventHandlers(
    bool need_touch_events) {
  if (content_view_core_)
    content_view_core_->HasTouchEventHandlers(need_touch_events);
}

// static
void RenderWidgetHostViewPort::GetDefaultScreenInfo(
    WebKit::WebScreenInfo* results) {
  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  results->rect = display.bounds();
  // TODO(husky): Remove any system controls from availableRect.
  results->availableRect = display.work_area();
  results->deviceScaleFactor = display.device_scale_factor();
  gfx::DeviceDisplayInfo info;
  results->depth = info.GetBitsPerPixel();
  results->depthPerComponent = info.GetBitsPerComponent();
  results->isMonochrome = (results->depthPerComponent == 0);
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
