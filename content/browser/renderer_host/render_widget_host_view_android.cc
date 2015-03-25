// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include <android/bitmap.h>

#include "base/android/build_info.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/worker_pool.h"
#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_renderer_layer.h"
#include "cc/layers/layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/latency_info_swap_promise.h"
#include "cc/output/viewport_selection_bound.h"
#include "cc/resources/single_release_callback.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/trees/layer_tree_host.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/android/composited_touch_handle_drawable.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/edge_effect.h"
#include "content/browser/android/edge_effect_l.h"
#include "content/browser/android/in_process/synchronous_compositor_impl.h"
#include "content/browser/android/overscroll_controller_android.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/frame_metadata_util.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_android.h"
#include "content/browser/renderer_host/input/web_input_event_builders_android.h"
#include "content/browser/renderer_host/input/web_input_event_util.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "skia/ext/image_operations.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/android/view_configuration.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/screen.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace content {

namespace {

void SatisfyCallback(cc::SurfaceManager* manager,
                     cc::SurfaceSequence sequence) {
  std::vector<uint32_t> sequences;
  sequences.push_back(sequence.sequence);
  manager->DidSatisfySequences(sequence.id_namespace, &sequences);
}

void RequireCallback(cc::SurfaceManager* manager,
                     cc::SurfaceId id,
                     cc::SurfaceSequence sequence) {
  cc::Surface* surface = manager->GetSurfaceForId(id);
  if (!surface) {
    LOG(ERROR) << "Attempting to require callback on nonexistent surface";
    return;
  }
  surface->AddDestructionDependency(sequence);
}

const int kUndefinedOutputSurfaceId = -1;

static const char kAsyncReadBackString[] = "Compositing.CopyFromSurfaceTime";

// Sends an acknowledgement to the renderer of a processed IME event.
void SendImeEventAck(RenderWidgetHostImpl* host) {
  host->Send(new ViewMsg_ImeEventAck(host->GetRoutingID()));
}

class GLHelperHolder
    : public blink::WebGraphicsContext3D::WebGraphicsContextLostCallback {
 public:
  static GLHelperHolder* Create();
  ~GLHelperHolder() override;

  void Initialize();

  // WebGraphicsContextLostCallback implementation.
  virtual void onContextLost() override;

  GLHelper* GetGLHelper() { return gl_helper_.get(); }
  bool IsLost() { return !context_.get() || context_->isContextLost(); }

 private:
  GLHelperHolder();
  static scoped_ptr<WebGraphicsContext3DCommandBufferImpl> CreateContext3D();

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context_;
  scoped_ptr<GLHelper> gl_helper_;

  DISALLOW_COPY_AND_ASSIGN(GLHelperHolder);
};

GLHelperHolder* GLHelperHolder::Create() {
  GLHelperHolder* holder = new GLHelperHolder;
  holder->Initialize();

  return holder;
}

GLHelperHolder::GLHelperHolder() {
}

GLHelperHolder::~GLHelperHolder() {
}

void GLHelperHolder::Initialize() {
  context_ = CreateContext3D();
  if (context_) {
    context_->setContextLostCallback(this);
    gl_helper_.reset(new GLHelper(context_->GetImplementation(),
                                  context_->GetContextSupport()));
  }
}

void GLHelperHolder::onContextLost() {
  // Need to post a task because the command buffer client cannot be deleted
  // from within this callback.
  LOG(ERROR) << "Context lost.";
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&RenderWidgetHostViewAndroid::OnContextLost));
}

scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
GLHelperHolder::CreateContext3D() {
  BrowserGpuChannelHostFactory* factory =
      BrowserGpuChannelHostFactory::instance();
  scoped_refptr<GpuChannelHost> gpu_channel_host(factory->GetGpuChannel());
  // GLHelper can only be used in asynchronous APIs for postprocessing after
  // Browser Compositor operations (i.e. readback).
  if (!gpu_channel_host.get()) {
    // The Browser Compositor is in charge of reestablishing the channel.
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  }

  blink::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  GURL url("chrome://gpu/RenderWidgetHostViewAndroid");
  static const size_t kBytesPerPixel = 4;
  gfx::DeviceDisplayInfo display_info;
  size_t full_screen_texture_size_in_bytes = display_info.GetDisplayHeight() *
                                             display_info.GetDisplayWidth() *
                                             kBytesPerPixel;
  WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits limits;
  limits.command_buffer_size = 64 * 1024;
  limits.start_transfer_buffer_size = 64 * 1024;
  limits.min_transfer_buffer_size = 64 * 1024;
  limits.max_transfer_buffer_size = std::min(
      3 * full_screen_texture_size_in_bytes, kDefaultMaxTransferBufferSize);
  limits.mapped_memory_reclaim_limit =
      WebGraphicsContext3DCommandBufferImpl::kNoLimit;
  bool lose_context_when_out_of_memory = false;
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
      new WebGraphicsContext3DCommandBufferImpl(
          0,  // offscreen
          url, gpu_channel_host.get(), attrs, lose_context_when_out_of_memory,
          limits, nullptr));
  if (context->InitializeOnCurrentThread()) {
    context->traceBeginCHROMIUM(
        "gpu_toplevel",
        base::StringPrintf("CmdBufferImageTransportFactory-%p",
                           context.get()).c_str());
  } else {
    context.reset();
  }

  return context.Pass();
}

// This can only be used for readback postprocessing. It may return null if the
// channel was lost and not reestablished yet.
GLHelper* GetPostReadbackGLHelper() {
  static GLHelperHolder* g_readback_helper_holder = nullptr;

  if (g_readback_helper_holder && g_readback_helper_holder->IsLost()) {
    delete g_readback_helper_holder;
    g_readback_helper_holder = nullptr;
  }

  if (!g_readback_helper_holder)
    g_readback_helper_holder = GLHelperHolder::Create();

  return g_readback_helper_holder->GetGLHelper();
}

void CopyFromCompositingSurfaceFinished(
    ReadbackRequestCallback& callback,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    scoped_ptr<SkBitmap> bitmap,
    const base::TimeTicks& start_time,
    scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock,
    bool result) {
  TRACE_EVENT0(
      "cc", "RenderWidgetHostViewAndroid::CopyFromCompositingSurfaceFinished");
  bitmap_pixels_lock.reset();
  uint32 sync_point = 0;
  if (result) {
    GLHelper* gl_helper = GetPostReadbackGLHelper();
    if (gl_helper)
      sync_point = gl_helper->InsertSyncPoint();
  }
  bool lost_resource = sync_point == 0;
  release_callback->Run(sync_point, lost_resource);
  UMA_HISTOGRAM_TIMES(kAsyncReadBackString,
                      base::TimeTicks::Now() - start_time);
  ReadbackResponse response = result ? READBACK_SUCCESS : READBACK_FAILED;
  callback.Run(*bitmap, response);
}

ui::LatencyInfo CreateLatencyInfo(const blink::WebInputEvent& event) {
  ui::LatencyInfo latency_info;
  // The latency number should only be added if the timestamp is valid.
  if (event.timeStampSeconds) {
    const int64 time_micros = static_cast<int64>(
        event.timeStampSeconds * base::Time::kMicrosecondsPerSecond);
    latency_info.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
        0,
        0,
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(time_micros),
        1);
  }
  return latency_info;
}

scoped_ptr<ui::TouchSelectionController> CreateSelectionController(
    ui::TouchSelectionControllerClient* client,
    ContentViewCore* content_view_core) {
  DCHECK(client);
  DCHECK(content_view_core);
  int tap_timeout_ms = gfx::ViewConfiguration::GetTapTimeoutInMs();
  int touch_slop_pixels = gfx::ViewConfiguration::GetTouchSlopInPixels();
  bool show_on_tap_for_empty_editable = false;
  return make_scoped_ptr(new ui::TouchSelectionController(
      client,
      base::TimeDelta::FromMilliseconds(tap_timeout_ms),
      touch_slop_pixels / content_view_core->GetDpiScale(),
      show_on_tap_for_empty_editable));
}

scoped_ptr<OverscrollControllerAndroid> CreateOverscrollController(
    ContentViewCore* content_view_core) {
  DCHECK(content_view_core);
  ui::WindowAndroid* window = content_view_core->GetWindowAndroid();
  DCHECK(window);
  ui::WindowAndroidCompositor* compositor = window->GetCompositor();
  DCHECK(compositor);
  return make_scoped_ptr(new OverscrollControllerAndroid(
      content_view_core->GetWebContents(),
      compositor,
      content_view_core->GetDpiScale()));
}

ui::GestureProvider::Config CreateGestureProviderConfig() {
  ui::GestureProvider::Config config = ui::GetGestureProviderConfig(
      ui::GestureProviderConfigType::CURRENT_PLATFORM);
  config.disable_click_delay =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableClickDelay);
  return config;
}

ui::SelectionBound::Type ConvertSelectionBoundType(
    cc::SelectionBoundType type) {
  switch (type) {
    case cc::SELECTION_BOUND_LEFT:
      return ui::SelectionBound::LEFT;
    case cc::SELECTION_BOUND_RIGHT:
      return ui::SelectionBound::RIGHT;
    case cc::SELECTION_BOUND_CENTER:
      return ui::SelectionBound::CENTER;
    case cc::SELECTION_BOUND_EMPTY:
      return ui::SelectionBound::EMPTY;
  }
  NOTREACHED() << "Unknown selection bound type";
  return ui::SelectionBound::EMPTY;
}

ui::SelectionBound ConvertSelectionBound(
    const cc::ViewportSelectionBound& bound) {
  ui::SelectionBound ui_bound;
  ui_bound.set_type(ConvertSelectionBoundType(bound.type));
  ui_bound.set_visible(bound.visible);
  if (ui_bound.type() != ui::SelectionBound::EMPTY)
    ui_bound.SetEdge(bound.edge_top, bound.edge_bottom);
  return ui_bound;
}

}  // anonymous namespace

ReadbackRequest::ReadbackRequest(float scale,
                                 SkColorType color_type,
                                 gfx::Rect src_subrect,
                                 ReadbackRequestCallback& result_callback)
    : scale_(scale),
      color_type_(color_type),
      src_subrect_(src_subrect),
      result_callback_(result_callback) {
}

ReadbackRequest::ReadbackRequest() {
}

ReadbackRequest::~ReadbackRequest() {
}

RenderWidgetHostViewAndroid::LastFrameInfo::LastFrameInfo(
    uint32 output_id,
    scoped_ptr<cc::CompositorFrame> output_frame)
    : output_surface_id(output_id), frame(output_frame.Pass()) {}

RenderWidgetHostViewAndroid::LastFrameInfo::~LastFrameInfo() {}

void RenderWidgetHostViewAndroid::OnContextLost() {
  scoped_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHostImpl::GetAllRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    if (widget->GetView()) {
      static_cast<RenderWidgetHostViewAndroid*>(widget->GetView())
          ->OnLostResources();
    }
  }
}

RenderWidgetHostViewAndroid::RenderWidgetHostViewAndroid(
    RenderWidgetHostImpl* widget_host,
    ContentViewCoreImpl* content_view_core)
    : host_(widget_host),
      outstanding_vsync_requests_(0),
      is_showing_(!widget_host->is_hidden()),
      content_view_core_(NULL),
      ime_adapter_android_(this),
      cached_background_color_(SK_ColorWHITE),
      last_output_surface_id_(kUndefinedOutputSurfaceId),
      gesture_provider_(CreateGestureProviderConfig(), this),
      stylus_text_selector_(this),
      accelerated_surface_route_id_(0),
      using_browser_compositor_(CompositorImpl::IsInitialized()),
      frame_evictor_(new DelegatedFrameEvictor(this)),
      locks_on_frame_count_(0),
      observing_root_window_(false),
      weak_ptr_factory_(this) {
  host_->SetView(this);
  SetContentViewCore(content_view_core);
}

RenderWidgetHostViewAndroid::~RenderWidgetHostViewAndroid() {
  SetContentViewCore(NULL);
  DCHECK(ack_callbacks_.empty());
  DCHECK(readbacks_waiting_for_frame_.empty());
  if (resource_collection_.get())
    resource_collection_->SetClient(NULL);
  DCHECK(!surface_factory_);
  DCHECK(surface_id_.is_null());
}


bool RenderWidgetHostViewAndroid::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetHostViewAndroid, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartContentIntent, OnStartContentIntent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidChangeBodyBackgroundColor,
                        OnDidChangeBodyBackgroundColor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetNeedsBeginFrames,
                        OnSetNeedsBeginFrames)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TextInputStateChanged,
                        OnTextInputStateChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SmartClipDataExtracted,
                        OnSmartClipDataExtracted)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowUnhandledTapUIIfNeeded,
                        OnShowUnhandledTapUIIfNeeded)
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

void RenderWidgetHostViewAndroid::WasResized() {
  host_->WasResized();
}

void RenderWidgetHostViewAndroid::SetSize(const gfx::Size& size) {
  // Ignore the given size as only the Java code has the power to
  // resize the view on Android.
  default_size_ = size;
}

void RenderWidgetHostViewAndroid::SetBounds(const gfx::Rect& rect) {
  SetSize(rect.size());
}

void RenderWidgetHostViewAndroid::AbortPendingReadbackRequests() {
  while (!readbacks_waiting_for_frame_.empty()) {
    ReadbackRequest& readback_request = readbacks_waiting_for_frame_.front();
    readback_request.GetResultCallback().Run(SkBitmap(), READBACK_FAILED);
    readbacks_waiting_for_frame_.pop();
  }
}

void RenderWidgetHostViewAndroid::GetScaledContentBitmap(
    float scale,
    SkColorType color_type,
    gfx::Rect src_subrect,
    ReadbackRequestCallback& result_callback) {
  if (!host_ || host_->is_hidden()) {
    result_callback.Run(SkBitmap(), READBACK_NOT_SUPPORTED);
    return;
  }
  if (!IsSurfaceAvailableForCopy()) {
    // The view is visible, probably the frame has not yet arrived.
    // Just add the ReadbackRequest to queue and wait for frame arrival
    // to get this request processed.
    readbacks_waiting_for_frame_.push(
        ReadbackRequest(scale, color_type, src_subrect, result_callback));
    return;
  }

  gfx::Size bounds = layer_->bounds();
  if (src_subrect.IsEmpty())
    src_subrect = gfx::Rect(bounds);
  DCHECK_LE(src_subrect.width() + src_subrect.x(), bounds.width());
  DCHECK_LE(src_subrect.height() + src_subrect.y(), bounds.height());
  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  float device_scale_factor = display.device_scale_factor();
  DCHECK_GT(device_scale_factor, 0);
  gfx::Size dst_size(
      gfx::ToCeiledSize(gfx::ScaleSize(bounds, scale / device_scale_factor)));
  CopyFromCompositingSurface(
      src_subrect, dst_size, result_callback, color_type);
}

scoped_refptr<cc::Layer> RenderWidgetHostViewAndroid::CreateDelegatedLayer()
    const {
  scoped_refptr<cc::Layer> delegated_layer;
  if (!surface_id_.is_null()) {
    cc::SurfaceManager* manager = CompositorImpl::GetSurfaceManager();
    DCHECK(manager);
    // manager must outlive compositors using it.
    scoped_refptr<cc::SurfaceLayer> surface_layer = cc::SurfaceLayer::Create(
        base::Bind(&SatisfyCallback, base::Unretained(manager)),
        base::Bind(&RequireCallback, base::Unretained(manager)));
    surface_layer->SetSurfaceId(surface_id_, 1.f, texture_size_in_layer_);
    delegated_layer = surface_layer;
  } else {
    DCHECK(frame_provider_.get());
    delegated_layer = cc::DelegatedRendererLayer::Create(frame_provider_);
  }
  delegated_layer->SetBounds(content_size_in_layer_);
  delegated_layer->SetIsDrawable(true);
  delegated_layer->SetContentsOpaque(true);

  return delegated_layer;
}

bool RenderWidgetHostViewAndroid::HasValidFrame() const {
  if (!content_view_core_)
    return false;
  if (!layer_.get())
    return false;

  if (texture_size_in_layer_.IsEmpty())
    return false;
  // This tell us whether a valid frame has arrived or not.
  if (!frame_evictor_->HasFrame())
    return false;

  return true;
}

gfx::Vector2dF RenderWidgetHostViewAndroid::GetLastScrollOffset() const {
  return last_scroll_offset_;
}

gfx::NativeView RenderWidgetHostViewAndroid::GetNativeView() const {
  return content_view_core_->GetViewAndroid();
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
    const std::vector<WebPluginGeometry>& moves) {
  // We don't have plugin windows on Android. Do nothing. Note: this is called
  // from RenderWidgetHost::OnUpdateRect which is itself invoked while
  // processing the corresponding message from Renderer.
}

void RenderWidgetHostViewAndroid::Focus() {
  host_->Focus();
  host_->SetInputMethodActive(true);
  if (overscroll_controller_)
    overscroll_controller_->Enable();
}

void RenderWidgetHostViewAndroid::Blur() {
  host_->SetInputMethodActive(false);
  host_->Blur();
  if (overscroll_controller_)
    overscroll_controller_->Disable();
}

bool RenderWidgetHostViewAndroid::HasFocus() const {
  if (!content_view_core_)
    return false;  // ContentViewCore not created yet.

  return content_view_core_->HasFocus();
}

bool RenderWidgetHostViewAndroid::IsSurfaceAvailableForCopy() const {
  return HasValidFrame();
}

void RenderWidgetHostViewAndroid::Show() {
  if (is_showing_)
    return;

  is_showing_ = true;
  if (layer_.get())
    layer_->SetHideLayerAndSubtree(false);

  if (overscroll_controller_)
    overscroll_controller_->Enable();

  frame_evictor_->SetVisible(true);

  if (!host_ || !host_->is_hidden())
    return;

  host_->WasShown(ui::LatencyInfo());

  if (content_view_core_) {
    StartObservingRootWindow();
    RequestVSyncUpdate(BEGIN_FRAME);
  }
}

void RenderWidgetHostViewAndroid::Hide() {
  if (!is_showing_)
    return;

  is_showing_ = false;
  if (layer_.get() && locks_on_frame_count_ == 0)
    layer_->SetHideLayerAndSubtree(true);

  if (overscroll_controller_)
    overscroll_controller_->Disable();

  frame_evictor_->SetVisible(false);
  // We don't know if we will ever get a frame if we are hiding the renderer, so
  // we need to cancel all requests
  AbortPendingReadbackRequests();

  RunAckCallbacks(cc::SurfaceDrawStatus::DRAW_SKIPPED);

  if (!host_ || host_->is_hidden())
    return;

  // Inform the renderer that we are being hidden so it can reduce its resource
  // utilization.
  host_->WasHidden();

  StopObservingRootWindow();
}

bool RenderWidgetHostViewAndroid::IsShowing() {
  // ContentViewCoreImpl represents the native side of the Java
  // ContentViewCore.  It being NULL means that it is not attached
  // to the View system yet, so we treat this RWHVA as hidden.
  return is_showing_ && content_view_core_;
}

void RenderWidgetHostViewAndroid::LockCompositingSurface() {
  DCHECK(HasValidFrame());
  DCHECK(host_);
  DCHECK(frame_evictor_->HasFrame());
  frame_evictor_->LockFrame();
  locks_on_frame_count_++;
}

void RenderWidgetHostViewAndroid::UnlockCompositingSurface() {
  if (!frame_evictor_->HasFrame() || locks_on_frame_count_ == 0)
    return;

  DCHECK(HasValidFrame());
  frame_evictor_->UnlockFrame();
  locks_on_frame_count_--;

  if (locks_on_frame_count_ == 0) {
    if (last_frame_info_) {
      InternalSwapCompositorFrame(last_frame_info_->output_surface_id,
                                  last_frame_info_->frame.Pass());
      last_frame_info_.reset();
    }

    if (!is_showing_ && layer_.get())
      layer_->SetHideLayerAndSubtree(true);
  }
}

void RenderWidgetHostViewAndroid::SetTextSurroundingSelectionCallback(
    const TextSurroundingSelectionCallback& callback) {
  // Only one outstanding request is allowed at any given time.
  DCHECK(!callback.is_null());
  text_surrounding_selection_callback_ = callback;
}

void RenderWidgetHostViewAndroid::OnTextSurroundingSelectionResponse(
    const base::string16& content,
    size_t start_offset,
    size_t end_offset) {
  if (text_surrounding_selection_callback_.is_null())
    return;
  text_surrounding_selection_callback_.Run(content, start_offset, end_offset);
  text_surrounding_selection_callback_.Reset();
}

void RenderWidgetHostViewAndroid::OnShowUnhandledTapUIIfNeeded(int x_dip,
                                                               int y_dip) {
  if (!content_view_core_)
    return;
  // Validate the coordinates are within the viewport.
  gfx::Size viewport_size = content_view_core_->GetViewportSizeDip();
  if (x_dip < 0 || x_dip > viewport_size.width() ||
      y_dip < 0 || y_dip > viewport_size.height())
    return;
  content_view_core_->OnShowUnhandledTapUIIfNeeded(x_dip, y_dip);
}

void RenderWidgetHostViewAndroid::ReleaseLocksOnSurface() {
  if (!frame_evictor_->HasFrame()) {
    DCHECK_EQ(locks_on_frame_count_, 0u);
    return;
  }
  while (locks_on_frame_count_ > 0) {
    UnlockCompositingSurface();
  }
  RunAckCallbacks(cc::SurfaceDrawStatus::DRAW_SKIPPED);
}

gfx::Rect RenderWidgetHostViewAndroid::GetViewBounds() const {
  if (!content_view_core_)
    return gfx::Rect(default_size_);

  return gfx::Rect(content_view_core_->GetViewSize());
}

gfx::Size RenderWidgetHostViewAndroid::GetPhysicalBackingSize() const {
  if (!content_view_core_)
    return gfx::Size();

  return content_view_core_->GetPhysicalBackingSize();
}

bool RenderWidgetHostViewAndroid::DoTopControlsShrinkBlinkSize() const {
  if (!content_view_core_)
    return false;

  // Whether or not Blink's viewport size should be shrunk by the height of the
  // URL-bar.
  return content_view_core_->DoTopControlsShrinkBlinkSize();
}

float RenderWidgetHostViewAndroid::GetTopControlsHeight() const {
  if (!content_view_core_)
    return 0.f;

  // The height of the top controls.
  return content_view_core_->GetTopControlsHeightDip();
}

void RenderWidgetHostViewAndroid::UpdateCursor(const WebCursor& cursor) {
  // There are no cursors on Android.
}

void RenderWidgetHostViewAndroid::SetIsLoading(bool is_loading) {
  // Do nothing. The UI notification is handled through ContentViewClient which
  // is TabContentsDelegate.
}

void RenderWidgetHostViewAndroid::TextInputTypeChanged(
    ui::TextInputType type,
    ui::TextInputMode input_mode,
    bool can_compose_inline,
    int flags) {
  // Unused on Android, which uses OnTextInputChanged instead.
}

long RenderWidgetHostViewAndroid::GetNativeImeAdapter() {
  return reinterpret_cast<intptr_t>(&ime_adapter_android_);
}

void RenderWidgetHostViewAndroid::OnTextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  if (selection_controller_) {
    // This call is semi-redundant with that in |OnFocusedNodeChanged|. The
    // latter is guaranteed to be called before |OnSelectionBoundsChanged|,
    // while this call is present to ensure consistency with IME after
    // navigation and tab focus changes
    const bool is_editable_node = params.type != ui::TEXT_INPUT_TYPE_NONE;
    selection_controller_->OnSelectionEditable(is_editable_node);
  }

  // If the change is not originated from IME (e.g. Javascript, autofill),
  // send back the renderer an acknowledgement, regardless of how we exit from
  // this method.
  base::ScopedClosureRunner ack_caller;
  if (params.is_non_ime_change)
    ack_caller.Reset(base::Bind(&SendImeEventAck, host_));

  if (!IsShowing())
    return;

  content_view_core_->UpdateImeAdapter(
      GetNativeImeAdapter(),
      static_cast<int>(params.type), params.flags,
      params.value, params.selection_start, params.selection_end,
      params.composition_start, params.composition_end,
      params.show_ime_if_needed, params.is_non_ime_change);
}

void RenderWidgetHostViewAndroid::OnDidChangeBodyBackgroundColor(
    SkColor color) {
  if (cached_background_color_ == color)
    return;

  cached_background_color_ = color;
  if (content_view_core_)
    content_view_core_->OnBackgroundColorChanged(color);
}

void RenderWidgetHostViewAndroid::OnSetNeedsBeginFrames(bool enabled) {
  DCHECK(using_browser_compositor_);
  TRACE_EVENT1("cc", "RenderWidgetHostViewAndroid::OnSetNeedsBeginFrames",
               "enabled", enabled);
  if (enabled)
    RequestVSyncUpdate(PERSISTENT_BEGIN_FRAME);
  else
    outstanding_vsync_requests_ &= ~PERSISTENT_BEGIN_FRAME;
}

void RenderWidgetHostViewAndroid::OnStartContentIntent(
    const GURL& content_url) {
  if (content_view_core_)
    content_view_core_->StartContentIntent(content_url);
}

void RenderWidgetHostViewAndroid::OnSmartClipDataExtracted(
    const base::string16& text,
    const base::string16& html,
    const gfx::Rect rect) {
  if (content_view_core_)
    content_view_core_->OnSmartClipDataExtracted(text, html, rect);
}

bool RenderWidgetHostViewAndroid::OnTouchEvent(
    const ui::MotionEvent& event) {
  if (!host_)
    return false;

  if (selection_controller_ &&
      selection_controller_->WillHandleTouchEvent(event))
    return true;

  if (stylus_text_selector_.OnTouchEvent(event))
    return true;

  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(event);
  if (!result.succeeded)
    return false;

  blink::WebTouchEvent web_event =
      CreateWebTouchEventFromMotionEvent(event, result.did_generate_scroll);
  host_->ForwardTouchEventWithLatencyInfo(web_event,
                                          CreateLatencyInfo(web_event));

  // Send a proactive BeginFrame on the next vsync to reduce latency.
  // This is good enough as long as the first touch event has Begin semantics
  // and the actual scroll happens on the next vsync.
  if (observing_root_window_)
    RequestVSyncUpdate(BEGIN_FRAME);

  return true;
}

bool RenderWidgetHostViewAndroid::OnTouchHandleEvent(
    const ui::MotionEvent& event) {
  return selection_controller_ &&
         selection_controller_->WillHandleTouchEvent(event);
}

void RenderWidgetHostViewAndroid::ResetGestureDetection() {
  const ui::MotionEvent* current_down_event =
      gesture_provider_.GetCurrentDownEvent();
  if (current_down_event) {
    scoped_ptr<ui::MotionEvent> cancel_event = current_down_event->Cancel();
    OnTouchEvent(*cancel_event);
  }

  // A hard reset ensures prevention of any timer-based events.
  gesture_provider_.ResetDetection();
}

void RenderWidgetHostViewAndroid::OnDidNavigateMainFrameToNewPage() {
  ResetGestureDetection();
}

void RenderWidgetHostViewAndroid::SetDoubleTapSupportEnabled(bool enabled) {
  gesture_provider_.SetDoubleTapSupportForPlatformEnabled(enabled);
}

void RenderWidgetHostViewAndroid::SetMultiTouchZoomSupportEnabled(
    bool enabled) {
  gesture_provider_.SetMultiTouchZoomSupportEnabled(enabled);
}

void RenderWidgetHostViewAndroid::ImeCancelComposition() {
  ime_adapter_android_.CancelComposition();
}

void RenderWidgetHostViewAndroid::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  // TODO(yukawa): Implement this.
}

void RenderWidgetHostViewAndroid::FocusedNodeChanged(bool is_editable_node) {
  ime_adapter_android_.FocusedNodeChanged(is_editable_node);
  if (selection_controller_)
    selection_controller_->OnSelectionEditable(is_editable_node);
}

void RenderWidgetHostViewAndroid::RenderProcessGone(
    base::TerminationStatus status, int error_code) {
  Destroy();
}

void RenderWidgetHostViewAndroid::Destroy() {
  RemoveLayers();
  SetContentViewCore(NULL);

  if (!surface_id_.is_null()) {
    DCHECK(surface_factory_.get());
    surface_factory_->Destroy(surface_id_);
    surface_id_ = cc::SurfaceId();
  }
  surface_factory_.reset();

  // The RenderWidgetHost's destruction led here, so don't call it.
  host_ = NULL;

  delete this;
}

void RenderWidgetHostViewAndroid::SetTooltipText(
    const base::string16& tooltip_text) {
  // Tooltips don't makes sense on Android.
}

void RenderWidgetHostViewAndroid::SelectionChanged(const base::string16& text,
                                                   size_t offset,
                                                   const gfx::Range& range) {
  RenderWidgetHostViewBase::SelectionChanged(text, offset, range);

  if (selection_controller_)
    selection_controller_->OnSelectionEmpty(text.empty());

  if (!content_view_core_)
    return;
  if (range.is_empty()) {
    content_view_core_->OnSelectionChanged("");
    return;
  }

  DCHECK(!text.empty());
  size_t pos = range.GetMin() - offset;
  size_t n = range.length();

  DCHECK(pos + n <= text.length()) << "The text can not fully cover range.";
  if (pos >= text.length()) {
    NOTREACHED() << "The text can not cover range.";
    return;
  }

  std::string utf8_selection = base::UTF16ToUTF8(text.substr(pos, n));

  content_view_core_->OnSelectionChanged(utf8_selection);
}

void RenderWidgetHostViewAndroid::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  NOTREACHED() << "Selection bounds should be routed through the compositor.";
}

void RenderWidgetHostViewAndroid::SetBackgroundColor(SkColor color) {
  RenderWidgetHostViewBase::SetBackgroundColor(color);
  host_->SetBackgroundOpaque(GetBackgroundOpaque());
  OnDidChangeBodyBackgroundColor(color);
}

void RenderWidgetHostViewAndroid::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    ReadbackRequestCallback& callback,
    const SkColorType color_type) {
  TRACE_EVENT0("cc", "RenderWidgetHostViewAndroid::CopyFromCompositingSurface");
  if (!host_ || host_->is_hidden()) {
    callback.Run(SkBitmap(), READBACK_SURFACE_UNAVAILABLE);
    return;
  }
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (using_browser_compositor_ && !IsSurfaceAvailableForCopy()) {
    callback.Run(SkBitmap(), READBACK_NOT_SUPPORTED);
    return;
  }
  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  float device_scale_factor = display.device_scale_factor();
  gfx::Size dst_size_in_pixel =
      gfx::ConvertRectToPixel(device_scale_factor, gfx::Rect(dst_size)).size();
  gfx::Rect src_subrect_in_pixel =
      gfx::ConvertRectToPixel(device_scale_factor, src_subrect);

  if (!using_browser_compositor_) {
    SynchronousCopyContents(src_subrect_in_pixel, dst_size_in_pixel, callback,
                            color_type);
    UMA_HISTOGRAM_TIMES("Compositing.CopyFromSurfaceTimeSynchronous",
                        base::TimeTicks::Now() - start_time);
    return;
  }

  scoped_ptr<cc::CopyOutputRequest> request;
  scoped_refptr<cc::Layer> readback_layer;
  DCHECK(content_view_core_);
  DCHECK(content_view_core_->GetWindowAndroid());
  ui::WindowAndroidCompositor* compositor =
      content_view_core_->GetWindowAndroid()->GetCompositor();
  DCHECK(compositor);
  DCHECK(frame_provider_.get() || !surface_id_.is_null());
  scoped_refptr<cc::Layer> layer = CreateDelegatedLayer();
  DCHECK(layer);
  layer->SetHideLayerAndSubtree(true);
  compositor->AttachLayerForReadback(layer);

  readback_layer = layer;
  request = cc::CopyOutputRequest::CreateRequest(
      base::Bind(&RenderWidgetHostViewAndroid::
                     PrepareTextureCopyOutputResultForDelegatedReadback,
                 dst_size_in_pixel,
                 color_type,
                 start_time,
                 readback_layer,
                 callback));
  if (!src_subrect_in_pixel.IsEmpty())
    request->set_area(src_subrect_in_pixel);
  readback_layer->RequestCopyOfOutput(request.Pass());
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
    const gfx::Rect& rect_pixels, const SkBitmap& zoomed_bitmap) {
  if (!content_view_core_)
    return;

  content_view_core_->ShowDisambiguationPopup(rect_pixels, zoomed_bitmap);
}

scoped_ptr<SyntheticGestureTarget>
RenderWidgetHostViewAndroid::CreateSyntheticGestureTarget() {
  return scoped_ptr<SyntheticGestureTarget>(new SyntheticGestureTargetAndroid(
      host_, content_view_core_->CreateTouchEventSynthesizer()));
}

void RenderWidgetHostViewAndroid::SendDelegatedFrameAck(
    uint32 output_surface_id) {
  DCHECK(host_);
  cc::CompositorFrameAck ack;
  if (!surface_returned_resources_.empty())
    ack.resources.swap(surface_returned_resources_);
  if (resource_collection_.get())
    resource_collection_->TakeUnusedResourcesForChildCompositor(&ack.resources);
  host_->Send(new ViewMsg_SwapCompositorFrameAck(host_->GetRoutingID(),
                                                 output_surface_id, ack));
}

void RenderWidgetHostViewAndroid::SendReturnedDelegatedResources(
    uint32 output_surface_id) {
  DCHECK(host_);
  cc::CompositorFrameAck ack;
  if (!surface_returned_resources_.empty()) {
    ack.resources.swap(surface_returned_resources_);
  } else {
    DCHECK(resource_collection_.get());
    resource_collection_->TakeUnusedResourcesForChildCompositor(&ack.resources);
  }

  host_->Send(new ViewMsg_ReclaimCompositorResources(host_->GetRoutingID(),
                                                     output_surface_id, ack));
}

void RenderWidgetHostViewAndroid::UnusedResourcesAreAvailable() {
  DCHECK(surface_id_.is_null());
  if (ack_callbacks_.size())
    return;
  SendReturnedDelegatedResources(last_output_surface_id_);
}

void RenderWidgetHostViewAndroid::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  if (resources.empty())
    return;
  std::copy(resources.begin(), resources.end(),
            std::back_inserter(surface_returned_resources_));
  if (!ack_callbacks_.size())
    SendReturnedDelegatedResources(last_output_surface_id_);
}

void RenderWidgetHostViewAndroid::DestroyDelegatedContent() {
  RemoveLayers();
  frame_provider_ = NULL;
  if (!surface_id_.is_null()) {
    DCHECK(surface_factory_.get());
    surface_factory_->Destroy(surface_id_);
    surface_id_ = cc::SurfaceId();
  }
  layer_ = NULL;
  // This gets called when ever any eviction, loosing resources, swapping
  // problems are encountered and so we abort any pending readbacks here.
  AbortPendingReadbackRequests();
}

void RenderWidgetHostViewAndroid::CheckOutputSurfaceChanged(
    uint32 output_surface_id) {
  if (output_surface_id == last_output_surface_id_)
    return;
  // Drop the cc::DelegatedFrameResourceCollection so that we will not return
  // any resources from the old output surface with the new output surface id.
  if (resource_collection_.get()) {
    resource_collection_->SetClient(NULL);
    if (resource_collection_->LoseAllResources())
      SendReturnedDelegatedResources(last_output_surface_id_);
    resource_collection_ = NULL;
  }
  DestroyDelegatedContent();
  surface_factory_.reset();
  if (!surface_returned_resources_.empty())
    SendReturnedDelegatedResources(last_output_surface_id_);

  last_output_surface_id_ = output_surface_id;
}

void RenderWidgetHostViewAndroid::SubmitFrame(
    scoped_ptr<cc::DelegatedFrameData> frame_data) {
  cc::SurfaceManager* manager = CompositorImpl::GetSurfaceManager();
  if (manager) {
    if (!surface_factory_) {
      id_allocator_ = CompositorImpl::CreateSurfaceIdAllocator();
      surface_factory_ = make_scoped_ptr(new cc::SurfaceFactory(manager, this));
    }
    if (surface_id_.is_null() ||
        texture_size_in_layer_ != current_surface_size_) {
      RemoveLayers();
      if (!surface_id_.is_null())
        surface_factory_->Destroy(surface_id_);
      surface_id_ = id_allocator_->GenerateId();
      surface_factory_->Create(surface_id_);
      layer_ = CreateDelegatedLayer();

      DCHECK(layer_);

      current_surface_size_ = texture_size_in_layer_;
      AttachLayers();
    }
    scoped_ptr<cc::CompositorFrame> compositor_frame =
        make_scoped_ptr(new cc::CompositorFrame());
    compositor_frame->delegated_frame_data = frame_data.Pass();

    cc::SurfaceFactory::DrawCallback ack_callback =
        base::Bind(&RenderWidgetHostViewAndroid::RunAckCallbacks,
                   weak_ptr_factory_.GetWeakPtr());
    surface_factory_->SubmitFrame(surface_id_, compositor_frame.Pass(),
                                  ack_callback);
  } else {
    if (!resource_collection_.get()) {
      resource_collection_ = new cc::DelegatedFrameResourceCollection;
      resource_collection_->SetClient(this);
    }
    if (!frame_provider_.get() ||
        texture_size_in_layer_ != frame_provider_->frame_size()) {
      RemoveLayers();
      frame_provider_ = new cc::DelegatedFrameProvider(
          resource_collection_.get(), frame_data.Pass());
      layer_ = cc::DelegatedRendererLayer::Create(frame_provider_);
      AttachLayers();
    } else {
      frame_provider_->SetFrameData(frame_data.Pass());
    }
  }
}

void RenderWidgetHostViewAndroid::SwapDelegatedFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::DelegatedFrameData> frame_data) {
  CheckOutputSurfaceChanged(output_surface_id);
  bool has_content = !texture_size_in_layer_.IsEmpty();

  // DelegatedRendererLayerImpl applies the inverse device_scale_factor of the
  // renderer frame, assuming that the browser compositor will scale
  // it back up to device scale.  But on Android we put our browser layers in
  // physical pixels and set our browser CC device_scale_factor to 1, so this
  // suppresses the transform.  This line may need to be removed when fixing
  // http://crbug.com/384134 or http://crbug.com/310763
  frame_data->device_scale_factor = 1.0f;

  if (!has_content) {
    DestroyDelegatedContent();
  } else {
    SubmitFrame(frame_data.Pass());
  }

  if (layer_.get()) {
    layer_->SetIsDrawable(true);
    layer_->SetContentsOpaque(true);
    layer_->SetBounds(content_size_in_layer_);
    layer_->SetNeedsDisplay();
  }

  base::Closure ack_callback =
      base::Bind(&RenderWidgetHostViewAndroid::SendDelegatedFrameAck,
                 weak_ptr_factory_.GetWeakPtr(),
                 output_surface_id);

  ack_callbacks_.push(ack_callback);
  if (host_->is_hidden())
    RunAckCallbacks(cc::SurfaceDrawStatus::DRAW_SKIPPED);
}

void RenderWidgetHostViewAndroid::ComputeContentsSize(
    const cc::CompositorFrameMetadata& frame_metadata) {
  // Calculate the content size.  This should be 0 if the texture_size is 0.
  gfx::Vector2dF offset;
  if (texture_size_in_layer_.IsEmpty())
    content_size_in_layer_ = gfx::Size();
  content_size_in_layer_ = gfx::ToCeiledSize(gfx::ScaleSize(
      frame_metadata.scrollable_viewport_size,
      frame_metadata.device_scale_factor * frame_metadata.page_scale_factor));

}

void RenderWidgetHostViewAndroid::InternalSwapCompositorFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  last_scroll_offset_ = frame->metadata.root_scroll_offset;
  if (!frame->delegated_frame_data) {
    LOG(ERROR) << "Non-delegated renderer path no longer supported";
    return;
  }

  if (locks_on_frame_count_ > 0) {
    DCHECK(HasValidFrame());
    RetainFrame(output_surface_id, frame.Pass());
    return;
  }

  if (layer_.get() && layer_->layer_tree_host()) {
    for (size_t i = 0; i < frame->metadata.latency_info.size(); i++) {
      scoped_ptr<cc::SwapPromise> swap_promise(
          new cc::LatencyInfoSwapPromise(frame->metadata.latency_info[i]));
      layer_->layer_tree_host()->QueueSwapPromise(swap_promise.Pass());
    }
  }

  DCHECK(!frame->delegated_frame_data->render_pass_list.empty());

  cc::RenderPass* root_pass =
      frame->delegated_frame_data->render_pass_list.back();
  texture_size_in_layer_ = root_pass->output_rect.size();
  ComputeContentsSize(frame->metadata);

  SwapDelegatedFrame(output_surface_id, frame->delegated_frame_data.Pass());
  frame_evictor_->SwappedFrame(!host_->is_hidden());

  // As the metadata update may trigger view invalidation, always call it after
  // any potential compositor scheduling.
  OnFrameMetadataUpdated(frame->metadata);
  // Check if we have any pending readbacks, see if we have a frame available
  // and process them here.
  if (!readbacks_waiting_for_frame_.empty()) {
    while (!readbacks_waiting_for_frame_.empty()) {
      ReadbackRequest& readback_request = readbacks_waiting_for_frame_.front();
      GetScaledContentBitmap(readback_request.GetScale(),
                             readback_request.GetColorFormat(),
                             readback_request.GetCaptureRect(),
                             readback_request.GetResultCallback());
      readbacks_waiting_for_frame_.pop();
    }
  }
}

void RenderWidgetHostViewAndroid::OnSwapCompositorFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  InternalSwapCompositorFrame(output_surface_id, frame.Pass());
}

void RenderWidgetHostViewAndroid::RetainFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  DCHECK(locks_on_frame_count_);

  // Store the incoming frame so that it can be swapped when all the locks have
  // been released. If there is already a stored frame, then replace and skip
  // the previous one but make sure we still eventually send the ACK. Holding
  // the ACK also blocks the renderer when its max_frames_pending is reached.
  if (last_frame_info_) {
    base::Closure ack_callback =
        base::Bind(&RenderWidgetHostViewAndroid::SendDelegatedFrameAck,
                   weak_ptr_factory_.GetWeakPtr(),
                   last_frame_info_->output_surface_id);

    ack_callbacks_.push(ack_callback);
  }

  last_frame_info_.reset(new LastFrameInfo(output_surface_id, frame.Pass()));
}

void RenderWidgetHostViewAndroid::SynchronousFrameMetadata(
    const cc::CompositorFrameMetadata& frame_metadata) {
  if (!content_view_core_)
    return;

  // This is a subset of OnSwapCompositorFrame() used in the synchronous
  // compositor flow.
  OnFrameMetadataUpdated(frame_metadata);
  ComputeContentsSize(frame_metadata);

  // DevTools ScreenCast support for Android WebView.
  WebContents* web_contents = content_view_core_->GetWebContents();
  if (DevToolsAgentHost::HasFor(web_contents)) {
    scoped_refptr<DevToolsAgentHost> dtah =
        DevToolsAgentHost::GetOrCreateFor(web_contents);
    // Unblock the compositor.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &RenderFrameDevToolsAgentHost::SynchronousSwapCompositorFrame,
            static_cast<RenderFrameDevToolsAgentHost*>(dtah.get()),
            frame_metadata));
  }
}

void RenderWidgetHostViewAndroid::SetOverlayVideoMode(bool enabled) {
  if (layer_.get())
    layer_->SetContentsOpaque(!enabled);
}

bool RenderWidgetHostViewAndroid::SupportsAnimation() const {
  // The synchronous (WebView) compositor does not have a proper browser
  // compositor with which to drive animations.
  return using_browser_compositor_;
}

void RenderWidgetHostViewAndroid::SetNeedsAnimate() {
  DCHECK(content_view_core_);
  DCHECK(using_browser_compositor_);
  content_view_core_->GetWindowAndroid()->SetNeedsAnimate();
}

void RenderWidgetHostViewAndroid::MoveCaret(const gfx::PointF& position) {
  MoveCaret(gfx::Point(position.x(), position.y()));
}

void RenderWidgetHostViewAndroid::MoveRangeSelectionExtent(
    const gfx::PointF& extent) {
  DCHECK(content_view_core_);
  content_view_core_->MoveRangeSelectionExtent(extent);
}

void RenderWidgetHostViewAndroid::SelectBetweenCoordinates(
    const gfx::PointF& base,
    const gfx::PointF& extent) {
  DCHECK(content_view_core_);
  content_view_core_->SelectBetweenCoordinates(base, extent);
}

void RenderWidgetHostViewAndroid::OnSelectionEvent(
    ui::SelectionEventType event,
    const gfx::PointF& position) {
  DCHECK(content_view_core_);
  // Showing the selection action bar can alter the current View coordinates in
  // such a way that the current MotionEvent stream is suddenly shifted in
  // space. Avoid the associated scroll jump by pre-emptively cancelling gesture
  // detection; scrolling after the selection is activated is unnecessary.
  if (event == ui::SelectionEventType::SELECTION_SHOWN)
    ResetGestureDetection();
  content_view_core_->OnSelectionEvent(event, position);
}

scoped_ptr<ui::TouchHandleDrawable>
RenderWidgetHostViewAndroid::CreateDrawable() {
  DCHECK(content_view_core_);
  if (!using_browser_compositor_)
    return content_view_core_->CreatePopupTouchHandleDrawable();

  return scoped_ptr<ui::TouchHandleDrawable>(new CompositedTouchHandleDrawable(
      content_view_core_->GetLayer().get(),
      content_view_core_->GetDpiScale(),
      // Use the activity context (instead of the application context) to ensure
      // proper handle theming.
      content_view_core_->GetContext().obj()));
}

void RenderWidgetHostViewAndroid::SynchronousCopyContents(
    const gfx::Rect& src_subrect_in_pixel,
    const gfx::Size& dst_size_in_pixel,
    ReadbackRequestCallback& callback,
    const SkColorType color_type) {
  gfx::Size input_size_in_pixel;
  if (src_subrect_in_pixel.IsEmpty())
    input_size_in_pixel = content_size_in_layer_;
  else
    input_size_in_pixel = src_subrect_in_pixel.size();

  gfx::Size output_size_in_pixel;
  if (dst_size_in_pixel.IsEmpty())
    output_size_in_pixel = input_size_in_pixel;
  else
    output_size_in_pixel = dst_size_in_pixel;
  int output_width = output_size_in_pixel.width();
  int output_height = output_size_in_pixel.height();

  SynchronousCompositor* compositor =
      SynchronousCompositorImpl::FromID(host_->GetProcess()->GetID(),
                                        host_->GetRoutingID());
  if (!compositor) {
    callback.Run(SkBitmap(), READBACK_FAILED);
    return;
  }

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(output_width,
                                       output_height,
                                       color_type,
                                       kPremul_SkAlphaType));
  SkCanvas canvas(bitmap);
  canvas.scale(
      (float)output_width / (float)input_size_in_pixel.width(),
      (float)output_height / (float)input_size_in_pixel.height());
  compositor->DemandDrawSw(&canvas);
  callback.Run(bitmap, READBACK_SUCCESS);
}

void RenderWidgetHostViewAndroid::OnFrameMetadataUpdated(
    const cc::CompositorFrameMetadata& frame_metadata) {
  bool is_mobile_optimized = IsMobileOptimizedFrame(frame_metadata);
  gesture_provider_.SetDoubleTapSupportForPageEnabled(!is_mobile_optimized);

  if (!content_view_core_)
    return;

  if (overscroll_controller_)
    overscroll_controller_->OnFrameMetadataUpdated(frame_metadata);

  if (selection_controller_) {
    selection_controller_->OnSelectionBoundsChanged(
        ConvertSelectionBound(frame_metadata.selection_start),
        ConvertSelectionBound(frame_metadata.selection_end));
  }

  // All offsets and sizes are in CSS pixels.
  content_view_core_->UpdateFrameInfo(
      frame_metadata.root_scroll_offset,
      frame_metadata.page_scale_factor,
      gfx::Vector2dF(frame_metadata.min_page_scale_factor,
                     frame_metadata.max_page_scale_factor),
      frame_metadata.root_layer_size,
      frame_metadata.scrollable_viewport_size,
      frame_metadata.location_bar_offset,
      frame_metadata.location_bar_content_translation,
      is_mobile_optimized);
#if defined(VIDEO_HOLE)
  if (host_ && host_->IsRenderView()) {
    RenderViewHostImpl* rvhi = static_cast<RenderViewHostImpl*>(
        RenderViewHost::From(host_));
    WebContentsImpl* web_contents_impl =
        static_cast<WebContentsImpl*>(WebContents::FromRenderViewHost(rvhi));
    if (web_contents_impl)
      web_contents_impl->media_web_contents_observer()->OnFrameInfoUpdated();
  }
#endif  // defined(VIDEO_HOLE)
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceInitialized(int route_id) {
  // TODO: remove need for the surface id here
  accelerated_surface_route_id_ = route_id;
}

void RenderWidgetHostViewAndroid::AttachLayers() {
  if (!content_view_core_)
    return;
  if (!layer_.get())
    return;

  content_view_core_->AttachLayer(layer_);
  if (overscroll_controller_)
    overscroll_controller_->Enable();
  layer_->SetHideLayerAndSubtree(!is_showing_);
}

void RenderWidgetHostViewAndroid::RemoveLayers() {
  if (!content_view_core_)
    return;

  if (!layer_.get())
    return;

  content_view_core_->RemoveLayer(layer_);
  if (overscroll_controller_)
    overscroll_controller_->Disable();
}

void RenderWidgetHostViewAndroid::RequestVSyncUpdate(uint32 requests) {
  // The synchronous compositor does not requre BeginFrame messages.
  if (!using_browser_compositor_)
    requests &= FLUSH_INPUT;

  bool should_request_vsync = !outstanding_vsync_requests_ && requests;
  outstanding_vsync_requests_ |= requests;
  // Note that if we're not currently observing the root window, outstanding
  // vsync requests will be pushed if/when we resume observing in
  // |StartObservingRootWindow()|.
  if (observing_root_window_ && should_request_vsync)
    content_view_core_->GetWindowAndroid()->RequestVSyncUpdate();
}

void RenderWidgetHostViewAndroid::StartObservingRootWindow() {
  DCHECK(content_view_core_);
  if (observing_root_window_)
    return;

  observing_root_window_ = true;
  content_view_core_->GetWindowAndroid()->AddObserver(this);

  // Clear existing vsync requests to allow a request to the new window.
  uint32 outstanding_vsync_requests = outstanding_vsync_requests_;
  outstanding_vsync_requests_ = 0;
  RequestVSyncUpdate(outstanding_vsync_requests);
}

void RenderWidgetHostViewAndroid::StopObservingRootWindow() {
  if (!content_view_core_) {
    DCHECK(!observing_root_window_);
    return;
  }

  if (!observing_root_window_)
    return;

  observing_root_window_ = false;
  content_view_core_->GetWindowAndroid()->RemoveObserver(this);
}

void RenderWidgetHostViewAndroid::SendBeginFrame(base::TimeTicks frame_time,
                                                 base::TimeDelta vsync_period) {
  TRACE_EVENT1("cc", "RenderWidgetHostViewAndroid::SendBeginFrame",
               "frame_time_us", frame_time.ToInternalValue());
  base::TimeTicks display_time = frame_time + vsync_period;

  base::TimeTicks deadline =
      display_time - host_->GetEstimatedBrowserCompositeTime();

  host_->Send(new ViewMsg_BeginFrame(
      host_->GetRoutingID(),
      cc::BeginFrameArgs::Create(BEGINFRAME_FROM_HERE, frame_time, deadline,
                                 vsync_period, cc::BeginFrameArgs::NORMAL)));
}

bool RenderWidgetHostViewAndroid::Animate(base::TimeTicks frame_time) {
  bool needs_animate = false;
  if (overscroll_controller_) {
    needs_animate |= overscroll_controller_->Animate(
        frame_time, content_view_core_->GetLayer().get());
  }
  if (selection_controller_)
    needs_animate |= selection_controller_->Animate(frame_time);
  return needs_animate;
}

void RenderWidgetHostViewAndroid::EvictDelegatedFrame() {
  if (layer_.get())
    DestroyDelegatedContent();
  frame_evictor_->DiscardedFrame();
  // We are evicting the delegated frame,
  // so there should be no pending readback requests
  DCHECK(readbacks_waiting_for_frame_.empty());
}

bool RenderWidgetHostViewAndroid::HasAcceleratedSurface(
    const gfx::Size& desired_size) {
  NOTREACHED();
  return false;
}

void RenderWidgetHostViewAndroid::GetScreenInfo(blink::WebScreenInfo* result) {
  // ScreenInfo isn't tied to the widget on Android. Always return the default.
  RenderWidgetHostViewBase::GetDefaultScreenInfo(result);
}

// TODO(jrg): Find out the implications and answer correctly here,
// as we are returning the WebView and not root window bounds.
gfx::Rect RenderWidgetHostViewAndroid::GetBoundsInRootWindow() {
  return GetViewBounds();
}

gfx::GLSurfaceHandle RenderWidgetHostViewAndroid::GetCompositingSurface() {
  gfx::GLSurfaceHandle handle =
      gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::NULL_TRANSPORT);
  if (using_browser_compositor_) {
    handle.parent_client_id =
        BrowserGpuChannelHostFactory::instance()->GetGpuChannelId();
  }
  return handle;
}

void RenderWidgetHostViewAndroid::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch, InputEventAckState ack_result) {
  const bool event_consumed = ack_result == INPUT_EVENT_ACK_STATE_CONSUMED;
  gesture_provider_.OnAsyncTouchEventAck(event_consumed);
}

void RenderWidgetHostViewAndroid::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  if (overscroll_controller_)
    overscroll_controller_->OnGestureEventAck(event, ack_result);

  if (content_view_core_)
    content_view_core_->OnGestureEventAck(event, ack_result);
}

InputEventAckState RenderWidgetHostViewAndroid::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  if (selection_controller_) {
    switch (input_event.type) {
      case blink::WebInputEvent::GestureLongPress:
        selection_controller_->OnLongPressEvent();
        break;
      case blink::WebInputEvent::GestureTap:
        selection_controller_->OnTapEvent();
        break;
      default:
        break;
    }
  }

  if (overscroll_controller_ &&
      blink::WebInputEvent::isGestureEventType(input_event.type) &&
      overscroll_controller_->WillHandleGestureEvent(
          static_cast<const blink::WebGestureEvent&>(input_event))) {
    return INPUT_EVENT_ACK_STATE_CONSUMED;
  }

  if (content_view_core_ &&
      content_view_core_->FilterInputEvent(input_event))
    return INPUT_EVENT_ACK_STATE_CONSUMED;

  if (!host_)
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;

  if (input_event.type == blink::WebInputEvent::GestureTapDown ||
      input_event.type == blink::WebInputEvent::TouchStart) {
    GpuDataManagerImpl* gpu_data = GpuDataManagerImpl::GetInstance();
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim && gpu_data && accelerated_surface_route_id_ &&
        gpu_data->IsDriverBugWorkaroundActive(gpu::WAKE_UP_GPU_BEFORE_DRAWING))
      shim->Send(
          new AcceleratedSurfaceMsg_WakeUpGpu(accelerated_surface_route_id_));
  }

  SynchronousCompositorImpl* compositor =
      SynchronousCompositorImpl::FromID(host_->GetProcess()->GetID(),
                                          host_->GetRoutingID());
  if (compositor)
    return compositor->HandleInputEvent(input_event);
  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

void RenderWidgetHostViewAndroid::OnSetNeedsFlushInput() {
  TRACE_EVENT0("input", "RenderWidgetHostViewAndroid::OnSetNeedsFlushInput");
  RequestVSyncUpdate(FLUSH_INPUT);
}

BrowserAccessibilityManager*
    RenderWidgetHostViewAndroid::CreateBrowserAccessibilityManager(
        BrowserAccessibilityDelegate* delegate) {
  // TODO(dmazzoni): Currently there can only be one
  // BrowserAccessibilityManager per ContentViewCore, so return NULL
  // if there's already a BrowserAccessibilityManager for the main
  // frame.  Eventually, in order to support cross-process iframes on
  // Android we'll need to add support for a
  // BrowserAccessibilityManager for a child frame.
  // http://crbug.com/423846
  if (!host_ || host_->GetRootBrowserAccessibilityManager())
    return NULL;

  base::android::ScopedJavaLocalRef<jobject> obj;
  if (content_view_core_)
    obj = content_view_core_->GetJavaObject();
  return new BrowserAccessibilityManagerAndroid(
      obj,
      BrowserAccessibilityManagerAndroid::GetEmptyDocument(),
      delegate);
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

void RenderWidgetHostViewAndroid::SendMouseEvent(
    const blink::WebMouseEvent& event) {
  if (host_)
    host_->ForwardMouseEvent(event);
}

void RenderWidgetHostViewAndroid::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  if (host_)
    host_->ForwardWheelEvent(event);
}

void RenderWidgetHostViewAndroid::SendGestureEvent(
    const blink::WebGestureEvent& event) {
  // Sending a gesture that may trigger overscroll should resume the effect.
  if (overscroll_controller_)
    overscroll_controller_->Enable();

  if (host_)
    host_->ForwardGestureEventWithLatencyInfo(event, CreateLatencyInfo(event));
}

void RenderWidgetHostViewAndroid::MoveCaret(const gfx::Point& point) {
  if (host_)
    host_->MoveCaret(point);
}

void RenderWidgetHostViewAndroid::DismissTextHandles() {
  if (selection_controller_)
    selection_controller_->HideAndDisallowShowingAutomatically();
}

void RenderWidgetHostViewAndroid::SetTextHandlesTemporarilyHidden(bool hidden) {
  if (selection_controller_)
    selection_controller_->SetTemporarilyHidden(hidden);
}

void RenderWidgetHostViewAndroid::OnShowingPastePopup(
    const gfx::PointF& point) {
  if (!selection_controller_)
    return;

  // As the paste popup may be triggered *before* the bounds and editability
  // of the region have been updated, explicitly set the properties now.
  // TODO(jdduke): Remove this workaround when auxiliary paste popup
  // notifications are no longer required, crbug.com/398170.
  ui::SelectionBound insertion_bound;
  insertion_bound.set_type(ui::SelectionBound::CENTER);
  insertion_bound.set_visible(true);
  insertion_bound.SetEdge(point, point);
  selection_controller_->HideAndDisallowShowingAutomatically();
  selection_controller_->OnSelectionEditable(true);
  selection_controller_->OnSelectionEmpty(true);
  selection_controller_->OnSelectionBoundsChanged(insertion_bound,
                                                  insertion_bound);
  selection_controller_->AllowShowingFromCurrentSelection();
}

SkColor RenderWidgetHostViewAndroid::GetCachedBackgroundColor() const {
  return cached_background_color_;
}

void RenderWidgetHostViewAndroid::DidOverscroll(
    const DidOverscrollParams& params) {
  if (!content_view_core_ || !layer_.get() || !is_showing_)
    return;

  if (overscroll_controller_)
    overscroll_controller_->OnOverscrolled(params);
}

void RenderWidgetHostViewAndroid::DidStopFlinging() {
  if (content_view_core_)
    content_view_core_->DidStopFlinging();
}

void RenderWidgetHostViewAndroid::SetContentViewCore(
    ContentViewCoreImpl* content_view_core) {
  RemoveLayers();
  StopObservingRootWindow();

  bool resize = false;
  if (content_view_core != content_view_core_) {
    overscroll_controller_.reset();
    selection_controller_.reset();
    ReleaseLocksOnSurface();
    resize = true;
  }

  content_view_core_ = content_view_core;

  BrowserAccessibilityManager* manager = NULL;
  if (host_)
    manager = host_->GetRootBrowserAccessibilityManager();
  if (manager) {
    base::android::ScopedJavaLocalRef<jobject> obj;
    if (content_view_core_)
      obj = content_view_core_->GetJavaObject();
    manager->ToBrowserAccessibilityManagerAndroid()->SetContentViewCore(obj);
  }

  AttachLayers();

  if (!content_view_core_)
    return;

  StartObservingRootWindow();

  if (resize)
    WasResized();

  if (!selection_controller_)
    selection_controller_ = CreateSelectionController(this, content_view_core_);

  if (!overscroll_controller_ &&
      content_view_core_->GetWindowAndroid()->GetCompositor()) {
    overscroll_controller_ = CreateOverscrollController(content_view_core_);
  }
}

void RenderWidgetHostViewAndroid::RunAckCallbacks(
    cc::SurfaceDrawStatus status) {
  while (!ack_callbacks_.empty()) {
    ack_callbacks_.front().Run();
    ack_callbacks_.pop();
  }
}

void RenderWidgetHostViewAndroid::OnGestureEvent(
    const ui::GestureEventData& gesture) {
  blink::WebGestureEvent web_gesture =
      CreateWebGestureEventFromGestureEventData(gesture);
  // TODO(jdduke): Remove this workaround after Android fixes UiAutomator to
  // stop providing shift meta values to synthetic MotionEvents. This prevents
  // unintended shift+click interpretation of all accessibility clicks.
  // See crbug.com/443247.
  if (web_gesture.type == blink::WebInputEvent::GestureTap &&
      web_gesture.modifiers == blink::WebInputEvent::ShiftKey) {
    web_gesture.modifiers = 0;
  }
  SendGestureEvent(web_gesture);
}

void RenderWidgetHostViewAndroid::OnCompositingDidCommit() {
  RunAckCallbacks(cc::SurfaceDrawStatus::DRAWN);
}

void RenderWidgetHostViewAndroid::OnAttachCompositor() {
  DCHECK(content_view_core_);
  if (!overscroll_controller_)
    overscroll_controller_ = CreateOverscrollController(content_view_core_);
}

void RenderWidgetHostViewAndroid::OnDetachCompositor() {
  DCHECK(content_view_core_);
  DCHECK(using_browser_compositor_);
  RunAckCallbacks(cc::SurfaceDrawStatus::DRAW_SKIPPED);
  overscroll_controller_.reset();
}

void RenderWidgetHostViewAndroid::OnVSync(base::TimeTicks frame_time,
                                          base::TimeDelta vsync_period) {
  TRACE_EVENT0("cc,benchmark", "RenderWidgetHostViewAndroid::OnVSync");
  if (!host_)
    return;

  const uint32 current_vsync_requests = outstanding_vsync_requests_;
  outstanding_vsync_requests_ = 0;

  if (current_vsync_requests & FLUSH_INPUT)
    host_->FlushInput();

  if (current_vsync_requests & BEGIN_FRAME ||
      current_vsync_requests & PERSISTENT_BEGIN_FRAME) {
    SendBeginFrame(frame_time, vsync_period);
  }

  if (current_vsync_requests & PERSISTENT_BEGIN_FRAME)
    RequestVSyncUpdate(PERSISTENT_BEGIN_FRAME);
}

void RenderWidgetHostViewAndroid::OnAnimate(base::TimeTicks begin_frame_time) {
  if (Animate(begin_frame_time))
    SetNeedsAnimate();
}

void RenderWidgetHostViewAndroid::OnLostResources() {
  ReleaseLocksOnSurface();
  if (layer_.get())
    DestroyDelegatedContent();
  DCHECK(ack_callbacks_.empty());
  // We should not loose a frame if we have readback requests pending.
  DCHECK(readbacks_waiting_for_frame_.empty());
}

// static
void
RenderWidgetHostViewAndroid::PrepareTextureCopyOutputResultForDelegatedReadback(
    const gfx::Size& dst_size_in_pixel,
    const SkColorType color_type,
    const base::TimeTicks& start_time,
    scoped_refptr<cc::Layer> readback_layer,
    ReadbackRequestCallback& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  readback_layer->RemoveFromParent();
  PrepareTextureCopyOutputResult(
      dst_size_in_pixel, color_type, start_time, callback, result.Pass());
}

// static
void RenderWidgetHostViewAndroid::PrepareTextureCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    const SkColorType color_type,
    const base::TimeTicks& start_time,
    ReadbackRequestCallback& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(callback, SkBitmap(), READBACK_FAILED));
  TRACE_EVENT0("cc",
               "RenderWidgetHostViewAndroid::PrepareTextureCopyOutputResult");

  if (!result->HasTexture() || result->IsEmpty() || result->size().IsEmpty())
    return;

  gfx::Size output_size_in_pixel;
  if (dst_size_in_pixel.IsEmpty())
    output_size_in_pixel = result->size();
  else
    output_size_in_pixel = dst_size_in_pixel;

  scoped_ptr<SkBitmap> bitmap(new SkBitmap);
  if (!bitmap->tryAllocPixels(SkImageInfo::Make(output_size_in_pixel.width(),
                                                output_size_in_pixel.height(),
                                                color_type,
                                                kOpaque_SkAlphaType))) {
    return;
  }

  GLHelper* gl_helper = GetPostReadbackGLHelper();
  if (!gl_helper || !gl_helper->IsReadbackConfigSupported(color_type))
    return;

  scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock(
      new SkAutoLockPixels(*bitmap));
  uint8* pixels = static_cast<uint8*>(bitmap->getPixels());

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());
  if (!texture_mailbox.IsTexture())
    return;

  ignore_result(scoped_callback_runner.Release());

  gl_helper->CropScaleReadbackAndCleanMailbox(
      texture_mailbox.mailbox(),
      texture_mailbox.sync_point(),
      result->size(),
      gfx::Rect(result->size()),
      output_size_in_pixel,
      pixels,
      color_type,
      base::Bind(&CopyFromCompositingSurfaceFinished,
                 callback,
                 base::Passed(&release_callback),
                 base::Passed(&bitmap),
                 start_time,
                 base::Passed(&bitmap_pixels_lock)),
      GLHelper::SCALER_QUALITY_GOOD);
}

SkColorType RenderWidgetHostViewAndroid::PreferredReadbackFormat() {
  // Define the criteria here. If say the 16 texture readback is
  // supported we should go with that (this degrades quality)
  // or stick back to the default format.
  if (base::SysInfo::IsLowEndDevice()) {
    // TODO(sievers): Cannot use GLHelper here. Instead remove this API
    // and have CopyFromCompositingSurface() fall back to RGB8 if 565 was
    // requested but is not supported.
    GLHelper* gl_helper = GetPostReadbackGLHelper();
    if (gl_helper && gl_helper->IsReadbackConfigSupported(kRGB_565_SkColorType))
      return kRGB_565_SkColorType;
  }
  return kN32_SkColorType;
}

void RenderWidgetHostViewAndroid::OnStylusSelectBegin(float x0,
                                                      float y0,
                                                      float x1,
                                                      float y1) {
  SelectBetweenCoordinates(gfx::PointF(x0, y0), gfx::PointF(x1, y1));
}

void RenderWidgetHostViewAndroid::OnStylusSelectUpdate(float x, float y) {
  MoveRangeSelectionExtent(gfx::PointF(x, y));
}

void RenderWidgetHostViewAndroid::OnStylusSelectEnd() {
  if (selection_controller_)
    selection_controller_->AllowShowingFromCurrentSelection();
}

void RenderWidgetHostViewAndroid::OnStylusSelectTap(base::TimeTicks time,
                                                    float x,
                                                    float y) {
  // Treat the stylus tap as a long press, activating either a word selection or
  // context menu depending on the targetted content.
  blink::WebGestureEvent long_press = WebGestureEventBuilder::Build(
      blink::WebInputEvent::GestureLongPress,
      (time - base::TimeTicks()).InSecondsF(), x, y);
  SendGestureEvent(long_press);
}

// static
void RenderWidgetHostViewBase::GetDefaultScreenInfo(
    blink::WebScreenInfo* results) {
  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  results->rect = display.bounds();
  // TODO(husky): Remove any system controls from availableRect.
  results->availableRect = display.work_area();
  results->deviceScaleFactor = display.device_scale_factor();
  results->orientationAngle = display.RotationAsDegree();
  results->orientationType =
      RenderWidgetHostViewBase::GetOrientationTypeForMobile(display);
  gfx::DeviceDisplayInfo info;
  results->depth = info.GetBitsPerPixel();
  results->depthPerComponent = info.GetBitsPerComponent();
  results->isMonochrome = (results->depthPerComponent == 0);
}

} // namespace content
