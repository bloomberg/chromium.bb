// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include <android/bitmap.h>
#include <utility>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/worker_pool.h"
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
#include "content/browser/android/overscroll_controller_android.h"
#include "content/browser/android/popup_touch_handle_drawable.h"
#include "content/browser/android/synchronous_compositor_base.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/media/android/media_web_contents_observer_android.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/frame_metadata_util.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_android.h"
#include "content/browser/renderer_host/input/ui_touch_selection_helper.h"
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
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "skia/ext/image_operations.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/events/blink/blink_event_util.h"
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

class GLHelperHolder
    : public blink::WebGraphicsContext3D::WebGraphicsContextLostCallback {
 public:
  static GLHelperHolder* Create();
  ~GLHelperHolder() override;

  void Initialize();

  // WebGraphicsContextLostCallback implementation.
  void onContextLost() override;

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
  context->SetContextType(BROWSER_OFFSCREEN_MAINTHREAD_CONTEXT);
  if (context->InitializeOnCurrentThread()) {
    context->traceBeginCHROMIUM(
        "gpu_toplevel",
        base::StringPrintf("CmdBufferImageTransportFactory-%p",
                           context.get()).c_str());
  } else {
    context.reset();
  }

  return context;
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
    const ReadbackRequestCallback& callback,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    scoped_ptr<SkBitmap> bitmap,
    const base::TimeTicks& start_time,
    scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock,
    bool result) {
  TRACE_EVENT0(
      "cc", "RenderWidgetHostViewAndroid::CopyFromCompositingSurfaceFinished");
  bitmap_pixels_lock.reset();
  gpu::SyncToken sync_token;
  if (result) {
    GLHelper* gl_helper = GetPostReadbackGLHelper();
    if (gl_helper)
      gl_helper->GenerateSyncToken(&sync_token);
  }
  const bool lost_resource = !sync_token.HasData();
  release_callback->Run(sync_token, lost_resource);
  UMA_HISTOGRAM_TIMES(kAsyncReadBackString,
                      base::TimeTicks::Now() - start_time);
  ReadbackResponse response = result ? READBACK_SUCCESS : READBACK_FAILED;
  callback.Run(*bitmap, response);
}

scoped_ptr<ui::TouchSelectionController> CreateSelectionController(
    ui::TouchSelectionControllerClient* client,
    ContentViewCore* content_view_core) {
  DCHECK(client);
  DCHECK(content_view_core);
  ui::TouchSelectionController::Config config;
  config.max_tap_duration = base::TimeDelta::FromMilliseconds(
      gfx::ViewConfiguration::GetLongPressTimeoutInMs());
  config.tap_slop = gfx::ViewConfiguration::GetTouchSlopInDips();
  config.show_on_tap_for_empty_editable = false;
  config.enable_adaptive_handle_orientation =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAdaptiveSelectionHandleOrientation);
  config.enable_longpress_drag_selection =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLongpressDragSelection);
  return make_scoped_ptr(new ui::TouchSelectionController(client, config));
}

scoped_ptr<OverscrollControllerAndroid> CreateOverscrollController(
    ContentViewCoreImpl* content_view_core) {
  return make_scoped_ptr(new OverscrollControllerAndroid(content_view_core));
}

gfx::RectF GetSelectionRect(const ui::TouchSelectionController& controller) {
  gfx::RectF rect = controller.GetRectBetweenBounds();
  if (rect.IsEmpty())
    return rect;

  rect.Union(controller.GetStartHandleRect());
  rect.Union(controller.GetEndHandleRect());
  return rect;
}

}  // anonymous namespace

RenderWidgetHostViewAndroid::LastFrameInfo::LastFrameInfo(
    uint32_t output_id,
    scoped_ptr<cc::CompositorFrame> output_frame)
    : output_surface_id(output_id), frame(std::move(output_frame)) {}

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
      is_window_visible_(true),
      is_window_activity_started_(true),
      content_view_core_(nullptr),
      content_view_core_window_android_(nullptr),
      ime_adapter_android_(this),
      cached_background_color_(SK_ColorWHITE),
      last_output_surface_id_(kUndefinedOutputSurfaceId),
      gesture_provider_(ui::GetGestureProviderConfig(
                            ui::GestureProviderConfigType::CURRENT_PLATFORM),
                        this),
      stylus_text_selector_(this),
      using_browser_compositor_(CompositorImpl::IsInitialized()),
      frame_evictor_(new DelegatedFrameEvictor(this)),
      locks_on_frame_count_(0),
      observing_root_window_(false),
      weak_ptr_factory_(this) {
  if (CompositorImpl::GetSurfaceManager())
    id_allocator_ = CompositorImpl::CreateSurfaceIdAllocator();
  host_->SetView(this);
  SetContentViewCore(content_view_core);
}

RenderWidgetHostViewAndroid::~RenderWidgetHostViewAndroid() {
  SetContentViewCore(NULL);
  DCHECK(ack_callbacks_.empty());
  DCHECK(!surface_factory_);
  DCHECK(surface_id_.is_null());
}

void RenderWidgetHostViewAndroid::Blur() {
  host_->Blur();
  if (overscroll_controller_)
    overscroll_controller_->Disable();
}

bool RenderWidgetHostViewAndroid::OnMessageReceived(
    const IPC::Message& message) {
  if (IPC_MESSAGE_ID_CLASS(message.type()) == SyncCompositorMsgStart) {
    return SyncCompositorOnMessageReceived(message);
  }
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetHostViewAndroid, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartContentIntent, OnStartContentIntent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetNeedsBeginFrames,
                        OnSetNeedsBeginFrames)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SmartClipDataExtracted,
                        OnSmartClipDataExtracted)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowUnhandledTapUIIfNeeded,
                        OnShowUnhandledTapUIIfNeeded)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool RenderWidgetHostViewAndroid::SyncCompositorOnMessageReceived(
    const IPC::Message& message) {
  DCHECK(!content_view_core_ || sync_compositor_) << !!content_view_core_;
  return sync_compositor_ && sync_compositor_->OnMessageReceived(message);
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

void RenderWidgetHostViewAndroid::GetScaledContentBitmap(
    float scale,
    SkColorType preferred_color_type,
    gfx::Rect src_subrect,
    const ReadbackRequestCallback& result_callback) {
  if (!host_ || host_->is_hidden() || !IsSurfaceAvailableForCopy()) {
    result_callback.Run(SkBitmap(), READBACK_SURFACE_UNAVAILABLE);
    return;
  }
  gfx::Size bounds = layer_->bounds();
  if (src_subrect.IsEmpty())
    src_subrect = gfx::Rect(bounds);
  DCHECK_LE(src_subrect.width() + src_subrect.x(), bounds.width());
  DCHECK_LE(src_subrect.height() + src_subrect.y(), bounds.height());
  const gfx::Display& display = gfx::Screen::GetScreen()->GetPrimaryDisplay();
  float device_scale_factor = display.device_scale_factor();
  DCHECK_GT(device_scale_factor, 0);
  gfx::Size dst_size(
      gfx::ScaleToCeiledSize(src_subrect.size(), scale / device_scale_factor));
  src_subrect = gfx::ConvertRectToDIP(device_scale_factor, src_subrect);

  CopyFromCompositingSurface(src_subrect, dst_size, result_callback,
                             preferred_color_type);
}

scoped_refptr<cc::Layer> RenderWidgetHostViewAndroid::CreateDelegatedLayer()
    const {
  scoped_refptr<cc::Layer> delegated_layer;
  DCHECK(!surface_id_.is_null());
  cc::SurfaceManager* manager = CompositorImpl::GetSurfaceManager();
  DCHECK(manager);
  // manager must outlive compositors using it.
  scoped_refptr<cc::SurfaceLayer> surface_layer = cc::SurfaceLayer::Create(
      Compositor::LayerSettings(),
      base::Bind(&SatisfyCallback, base::Unretained(manager)),
      base::Bind(&RequireCallback, base::Unretained(manager)));
  surface_layer->SetSurfaceId(surface_id_, 1.f, texture_size_in_layer_);
  delegated_layer = surface_layer;
  delegated_layer->SetBounds(texture_size_in_layer_);
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
    const std::vector<WebPluginGeometry>& moves) {
  // We don't have plugin windows on Android. Do nothing. Note: this is called
  // from RenderWidgetHost::OnUpdateRect which is itself invoked while
  // processing the corresponding message from Renderer.
}

void RenderWidgetHostViewAndroid::Focus() {
  host_->Focus();
  if (overscroll_controller_)
    overscroll_controller_->Enable();
  if (content_view_core_) {
    WebContentsImpl* web_contents_impl =
        static_cast<WebContentsImpl*>(content_view_core_->GetWebContents());
    if (web_contents_impl->ShowingInterstitialPage())
      content_view_core_->ForceUpdateImeAdapter(GetNativeImeAdapter());
  }
}

bool RenderWidgetHostViewAndroid::HasFocus() const {
  if (!content_view_core_)
    return false;  // ContentViewCore not created yet.

  return content_view_core_->HasFocus();
}

bool RenderWidgetHostViewAndroid::IsSurfaceAvailableForCopy() const {
  return !using_browser_compositor_ || HasValidFrame();
}

void RenderWidgetHostViewAndroid::Show() {
  if (is_showing_)
    return;

  is_showing_ = true;
  ShowInternal();
}

void RenderWidgetHostViewAndroid::Hide() {
  if (!is_showing_)
    return;

  is_showing_ = false;
  HideInternal();
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
                                  std::move(last_frame_info_->frame));
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

long RenderWidgetHostViewAndroid::GetNativeImeAdapter() {
  return reinterpret_cast<intptr_t>(&ime_adapter_android_);
}

void RenderWidgetHostViewAndroid::TextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  if (params.is_non_ime_change) {
    // Sends an acknowledgement to the renderer of a processed IME event.
    host_->Send(new InputMsg_ImeEventAck(host_->GetRoutingID()));
  }

  if (!IsShowing())
    return;

  content_view_core_->UpdateImeAdapter(
      GetNativeImeAdapter(),
      static_cast<int>(params.type), params.flags,
      params.value, params.selection_start, params.selection_end,
      params.composition_start, params.composition_end,
      params.show_ime_if_needed, params.is_non_ime_change);
}

void RenderWidgetHostViewAndroid::UpdateBackgroundColor(SkColor color) {
  if (cached_background_color_ == color)
    return;

  cached_background_color_ = color;
  if (content_view_core_)
    content_view_core_->OnBackgroundColorChanged(color);
}

void RenderWidgetHostViewAndroid::OnSetNeedsBeginFrames(bool enabled) {
  TRACE_EVENT1("cc", "RenderWidgetHostViewAndroid::OnSetNeedsBeginFrames",
               "enabled", enabled);
  if (enabled)
    RequestVSyncUpdate(PERSISTENT_BEGIN_FRAME);
  else
    outstanding_vsync_requests_ &= ~PERSISTENT_BEGIN_FRAME;
}

void RenderWidgetHostViewAndroid::OnStartContentIntent(
    const GURL& content_url, bool is_main_frame) {
  if (content_view_core_)
    content_view_core_->StartContentIntent(content_url, is_main_frame);
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

  // If a browser-based widget consumes the touch event, it's critical that
  // touch event interception be disabled. This avoids issues with
  // double-handling for embedder-detected gestures like side swipe.
  if (selection_controller_ &&
      selection_controller_->WillHandleTouchEvent(event)) {
    RequestDisallowInterceptTouchEvent();
    return true;
  }

  if (stylus_text_selector_.OnTouchEvent(event)) {
    RequestDisallowInterceptTouchEvent();
    return true;
  }

  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(event);
  if (!result.succeeded)
    return false;

  blink::WebTouchEvent web_event =
      ui::CreateWebTouchEventFromMotionEvent(event, result.did_generate_scroll);
  host_->ForwardTouchEventWithLatencyInfo(web_event, ui::LatencyInfo());

  // Send a proactive BeginFrame for this vsync to reduce scroll latency for
  // scroll-inducing touch events. Note that Android's Choreographer ensures
  // that BeginFrame requests made during ACTION_MOVE dispatch will be honored
  // in the same vsync phase.
  if (observing_root_window_ && result.did_generate_scroll)
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
  if (!current_down_event) {
    // A hard reset ensures prevention of any timer-based events that might fire
    // after a touch sequence has ended.
    gesture_provider_.ResetDetection();
    return;
  }

  scoped_ptr<ui::MotionEvent> cancel_event = current_down_event->Cancel();
  if (gesture_provider_.OnTouchEvent(*cancel_event).succeeded) {
    bool causes_scrolling = false;
    host_->ForwardTouchEventWithLatencyInfo(
        ui::CreateWebTouchEventFromMotionEvent(*cancel_event, causes_scrolling),
        ui::LatencyInfo());
  }
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
  UpdateBackgroundColor(color);
}

void RenderWidgetHostViewAndroid::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  TRACE_EVENT0("cc", "RenderWidgetHostViewAndroid::CopyFromCompositingSurface");
  if (!host_ || host_->is_hidden()) {
    callback.Run(SkBitmap(), READBACK_SURFACE_UNAVAILABLE);
    return;
  }
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (!IsSurfaceAvailableForCopy()) {
    callback.Run(SkBitmap(), READBACK_SURFACE_UNAVAILABLE);
    return;
  }
  const gfx::Display& display = gfx::Screen::GetScreen()->GetPrimaryDisplay();
  float device_scale_factor = display.device_scale_factor();
  gfx::Size dst_size_in_pixel =
      gfx::ConvertRectToPixel(device_scale_factor, gfx::Rect(dst_size)).size();
  gfx::Rect src_subrect_in_pixel =
      gfx::ConvertRectToPixel(device_scale_factor, src_subrect);

  if (!using_browser_compositor_) {
    SynchronousCopyContents(src_subrect_in_pixel, dst_size_in_pixel, callback,
                            preferred_color_type);
    UMA_HISTOGRAM_TIMES("Compositing.CopyFromSurfaceTimeSynchronous",
                        base::TimeTicks::Now() - start_time);
    return;
  }

  scoped_ptr<cc::CopyOutputRequest> request;
  scoped_refptr<cc::Layer> readback_layer;
  DCHECK(content_view_core_window_android_);
  ui::WindowAndroidCompositor* compositor =
      content_view_core_window_android_->GetCompositor();
  DCHECK(compositor);
  DCHECK(!surface_id_.is_null());
  scoped_refptr<cc::Layer> layer = CreateDelegatedLayer();
  DCHECK(layer);
  layer->SetHideLayerAndSubtree(true);
  compositor->AttachLayerForReadback(layer);

  readback_layer = layer;
  request = cc::CopyOutputRequest::CreateRequest(
      base::Bind(&RenderWidgetHostViewAndroid::
                     PrepareTextureCopyOutputResultForDelegatedReadback,
                 dst_size_in_pixel, preferred_color_type, start_time,
                 readback_layer, callback));
  if (!src_subrect_in_pixel.IsEmpty())
    request->set_area(src_subrect_in_pixel);
  readback_layer->RequestCopyOfOutput(std::move(request));
}

void RenderWidgetHostViewAndroid::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(const gfx::Rect&, bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(gfx::Rect(), false);
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
      host_, content_view_core_->CreateMotionEventSynthesizer()));
}

void RenderWidgetHostViewAndroid::SendDelegatedFrameAck(
    uint32_t output_surface_id) {
  DCHECK(host_);
  cc::CompositorFrameAck ack;
  if (!surface_returned_resources_.empty())
    ack.resources.swap(surface_returned_resources_);
  host_->Send(new ViewMsg_SwapCompositorFrameAck(host_->GetRoutingID(),
                                                 output_surface_id, ack));
}

void RenderWidgetHostViewAndroid::SendReturnedDelegatedResources(
    uint32_t output_surface_id) {
  DCHECK(host_);
  cc::CompositorFrameAck ack;
  DCHECK(!surface_returned_resources_.empty());
  ack.resources.swap(surface_returned_resources_);

  host_->Send(new ViewMsg_ReclaimCompositorResources(host_->GetRoutingID(),
                                                     output_surface_id, ack));
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

void RenderWidgetHostViewAndroid::SetBeginFrameSource(
    cc::SurfaceId surface_id,
    cc::BeginFrameSource* begin_frame_source) {
  // TODO(tansell): Hook this up.
}

void RenderWidgetHostViewAndroid::DestroyDelegatedContent() {
  RemoveLayers();
  if (!surface_id_.is_null()) {
    DCHECK(surface_factory_.get());
    surface_factory_->Destroy(surface_id_);
    surface_id_ = cc::SurfaceId();
  }
  layer_ = NULL;
}

void RenderWidgetHostViewAndroid::CheckOutputSurfaceChanged(
    uint32_t output_surface_id) {
  if (output_surface_id == last_output_surface_id_)
    return;
  DestroyDelegatedContent();
  surface_factory_.reset();
  if (!surface_returned_resources_.empty())
    SendReturnedDelegatedResources(last_output_surface_id_);

  last_output_surface_id_ = output_surface_id;
}

void RenderWidgetHostViewAndroid::SubmitCompositorFrame(
    scoped_ptr<cc::CompositorFrame> frame) {
  cc::SurfaceManager* manager = CompositorImpl::GetSurfaceManager();
  if (!surface_factory_) {
    surface_factory_ = make_scoped_ptr(new cc::SurfaceFactory(manager, this));
  }
  if (surface_id_.is_null() ||
      texture_size_in_layer_ != current_surface_size_ ||
      location_bar_content_translation_ !=
          frame->metadata.location_bar_content_translation ||
      current_viewport_selection_ != frame->metadata.selection) {
    RemoveLayers();
    if (!surface_id_.is_null())
      surface_factory_->Destroy(surface_id_);
    surface_id_ = id_allocator_->GenerateId();
    surface_factory_->Create(surface_id_);
    layer_ = CreateDelegatedLayer();

    DCHECK(layer_);

    current_surface_size_ = texture_size_in_layer_;
    location_bar_content_translation_ =
        frame->metadata.location_bar_content_translation;
    current_viewport_selection_ = frame->metadata.selection;
    AttachLayers();
  }

  cc::SurfaceFactory::DrawCallback ack_callback =
      base::Bind(&RenderWidgetHostViewAndroid::RunAckCallbacks,
                 weak_ptr_factory_.GetWeakPtr());
  surface_factory_->SubmitCompositorFrame(surface_id_, std::move(frame),
                                          ack_callback);
}

void RenderWidgetHostViewAndroid::SwapDelegatedFrame(
    uint32_t output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  CheckOutputSurfaceChanged(output_surface_id);
  bool has_content = !texture_size_in_layer_.IsEmpty();

  // DelegatedRendererLayerImpl applies the inverse device_scale_factor of the
  // renderer frame, assuming that the browser compositor will scale
  // it back up to device scale.  But on Android we put our browser layers in
  // physical pixels and set our browser CC device_scale_factor to 1, so this
  // suppresses the transform.  This line may need to be removed when fixing
  // http://crbug.com/384134 or http://crbug.com/310763
  frame->delegated_frame_data->device_scale_factor = 1.0f;

  base::Closure ack_callback =
      base::Bind(&RenderWidgetHostViewAndroid::SendDelegatedFrameAck,
                 weak_ptr_factory_.GetWeakPtr(),
                 output_surface_id);

  ack_callbacks_.push(ack_callback);

  if (!has_content) {
    DestroyDelegatedContent();
  } else {
    SubmitCompositorFrame(std::move(frame));
  }

  if (layer_.get()) {
    layer_->SetIsDrawable(true);
    layer_->SetContentsOpaque(true);
    layer_->SetBounds(texture_size_in_layer_);
  }

  if (host_->is_hidden())
    RunAckCallbacks(cc::SurfaceDrawStatus::DRAW_SKIPPED);
}

void RenderWidgetHostViewAndroid::InternalSwapCompositorFrame(
    uint32_t output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  last_scroll_offset_ = frame->metadata.root_scroll_offset;
  if (!frame->delegated_frame_data) {
    LOG(ERROR) << "Non-delegated renderer path no longer supported";
    return;
  }

  if (locks_on_frame_count_ > 0) {
    DCHECK(HasValidFrame());
    RetainFrame(output_surface_id, std::move(frame));
    return;
  }

  if (!CompositorImpl::GetSurfaceManager() && layer_.get() &&
      layer_->layer_tree_host()) {
    for (size_t i = 0; i < frame->metadata.latency_info.size(); i++) {
      scoped_ptr<cc::SwapPromise> swap_promise(
          new cc::LatencyInfoSwapPromise(frame->metadata.latency_info[i]));
      layer_->layer_tree_host()->QueueSwapPromise(std::move(swap_promise));
    }
  }

  DCHECK(!frame->delegated_frame_data->render_pass_list.empty());

  cc::RenderPass* root_pass =
      frame->delegated_frame_data->render_pass_list.back().get();
  texture_size_in_layer_ = root_pass->output_rect.size();

  cc::CompositorFrameMetadata metadata = frame->metadata;

  SwapDelegatedFrame(output_surface_id, std::move(frame));
  frame_evictor_->SwappedFrame(!host_->is_hidden());

  // As the metadata update may trigger view invalidation, always call it after
  // any potential compositor scheduling.
  OnFrameMetadataUpdated(metadata);
}

void RenderWidgetHostViewAndroid::OnSwapCompositorFrame(
    uint32_t output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  InternalSwapCompositorFrame(output_surface_id, std::move(frame));
}

void RenderWidgetHostViewAndroid::ClearCompositorFrame() {
  DestroyDelegatedContent();
}

void RenderWidgetHostViewAndroid::RetainFrame(
    uint32_t output_surface_id,
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

  last_frame_info_.reset(
      new LastFrameInfo(output_surface_id, std::move(frame)));
}

SynchronousCompositorBase*
RenderWidgetHostViewAndroid::GetSynchronousCompositor() {
  return sync_compositor_.get();
}

void RenderWidgetHostViewAndroid::SynchronousFrameMetadata(
    const cc::CompositorFrameMetadata& frame_metadata) {
  if (!content_view_core_)
    return;

  // This is a subset of OnSwapCompositorFrame() used in the synchronous
  // compositor flow.
  OnFrameMetadataUpdated(frame_metadata);

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
  DCHECK(content_view_core_window_android_);
  DCHECK(using_browser_compositor_);
  content_view_core_window_android_->SetNeedsAnimate();
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
    ui::SelectionEventType event) {
  DCHECK(content_view_core_);
  DCHECK(selection_controller_);
  // If a selection drag has started, it has taken over the active touch
  // sequence. Immediately cancel gesture detection and any downstream touch
  // listeners (e.g., web content) to communicate this transfer.
  if (event == ui::SELECTION_HANDLES_SHOWN &&
      gesture_provider_.GetCurrentDownEvent()) {
    ResetGestureDetection();
  }
  content_view_core_->OnSelectionEvent(
      event, selection_controller_->GetStartPosition(),
      GetSelectionRect(*selection_controller_));
}

scoped_ptr<ui::TouchHandleDrawable>
RenderWidgetHostViewAndroid::CreateDrawable() {
  DCHECK(content_view_core_);
  if (!using_browser_compositor_)
    return PopupTouchHandleDrawable::Create(content_view_core_);

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
    const ReadbackRequestCallback& callback,
    const SkColorType color_type) {
  gfx::Size input_size_in_pixel;
  if (src_subrect_in_pixel.IsEmpty())
    input_size_in_pixel = texture_size_in_layer_;
  else
    input_size_in_pixel = src_subrect_in_pixel.size();

  gfx::Size output_size_in_pixel;
  if (dst_size_in_pixel.IsEmpty())
    output_size_in_pixel = input_size_in_pixel;
  else
    output_size_in_pixel = dst_size_in_pixel;
  int output_width = output_size_in_pixel.width();
  int output_height = output_size_in_pixel.height();

  if (!sync_compositor_) {
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
  sync_compositor_->DemandDrawSw(&canvas);
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
    selection_controller_->OnSelectionEditable(
        frame_metadata.selection.is_editable);
    selection_controller_->OnSelectionEmpty(
        frame_metadata.selection.is_empty_text_form_control);
    selection_controller_->OnSelectionBoundsChanged(
        ConvertSelectionBound(frame_metadata.selection.start),
        ConvertSelectionBound(frame_metadata.selection.end));

    // Set parameters for adaptive handle orientation.
    gfx::SizeF viewport_size(frame_metadata.scrollable_viewport_size);
    viewport_size.Scale(frame_metadata.page_scale_factor);
    gfx::RectF viewport_rect(
        frame_metadata.location_bar_content_translation.x(),
        frame_metadata.location_bar_content_translation.y(),
        viewport_size.width(), viewport_size.height());
    selection_controller_->OnViewportChanged(viewport_rect);
  }

  UpdateBackgroundColor(frame_metadata.root_background_color);

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
  if (host_) {
    WebContents* web_contents =
        WebContents::FromRenderViewHost(RenderViewHostImpl::From(host_));
    if (web_contents) {
      MediaWebContentsObserverAndroid::FromWebContents(web_contents)
          ->OnFrameInfoUpdated();
    }
  }
#endif  // defined(VIDEO_HOLE)
}

void RenderWidgetHostViewAndroid::ShowInternal() {
  bool show = is_showing_ && is_window_activity_started_ && is_window_visible_;
  if (!show)
    return;

  if (!host_ || !host_->is_hidden())
    return;

  if (layer_.get())
    layer_->SetHideLayerAndSubtree(false);

  frame_evictor_->SetVisible(true);

  if (overscroll_controller_)
    overscroll_controller_->Enable();

  host_->WasShown(ui::LatencyInfo());

  if (content_view_core_) {
    StartObservingRootWindow();
    RequestVSyncUpdate(BEGIN_FRAME);
  }
}

void RenderWidgetHostViewAndroid::HideInternal() {
  DCHECK(!is_showing_ || !is_window_activity_started_ || !is_window_visible_)
      << "Hide called when the widget should be shown.";

  // Only preserve the frontbuffer if the activity was stopped while the
  // window is still visible. This avoids visual artificts when transitioning
  // between activities.
  bool hide_frontbuffer = is_window_activity_started_ || !is_window_visible_;

  // Only stop observing the root window if the widget has been explicitly
  // hidden and the frontbuffer is being cleared. This allows window visibility
  // notifications to eventually clear the frontbuffer.
  bool stop_observing_root_window = !is_showing_ && hide_frontbuffer;

  if (hide_frontbuffer) {
    if (layer_.get() && locks_on_frame_count_ == 0)
      layer_->SetHideLayerAndSubtree(true);

    frame_evictor_->SetVisible(false);
  }

  if (stop_observing_root_window) {
    DCHECK(!is_showing_);
    StopObservingRootWindow();
  }

  if (!host_ || host_->is_hidden())
    return;

  if (overscroll_controller_)
    overscroll_controller_->Disable();

  RunAckCallbacks(cc::SurfaceDrawStatus::DRAW_SKIPPED);

  // Inform the renderer that we are being hidden so it can reduce its resource
  // utilization.
  host_->WasHidden();
}

void RenderWidgetHostViewAndroid::AttachLayers() {
  if (!content_view_core_)
    return;
  if (!layer_.get())
    return;

  content_view_core_->AttachLayer(layer_);
  layer_->SetHideLayerAndSubtree(!is_showing_);
}

void RenderWidgetHostViewAndroid::RemoveLayers() {
  if (!content_view_core_)
    return;

  if (!layer_.get())
    return;

  content_view_core_->RemoveLayer(layer_);
}

void RenderWidgetHostViewAndroid::RequestVSyncUpdate(uint32_t requests) {
  bool should_request_vsync = !outstanding_vsync_requests_ && requests;
  outstanding_vsync_requests_ |= requests;

  // If the host has been hidden, defer vsync requests until it is shown
  // again via |Show()|.
  if (!host_ || host_->is_hidden())
    return;

  // Note that if we're not currently observing the root window, outstanding
  // vsync requests will be pushed if/when we resume observing in
  // |StartObservingRootWindow()|.
  if (observing_root_window_ && should_request_vsync)
    content_view_core_window_android_->RequestVSyncUpdate();
}

void RenderWidgetHostViewAndroid::StartObservingRootWindow() {
  DCHECK(content_view_core_window_android_);
  DCHECK(is_showing_);
  if (observing_root_window_)
    return;

  observing_root_window_ = true;
  content_view_core_window_android_->AddObserver(this);

  // Clear existing vsync requests to allow a request to the new window.
  uint32_t outstanding_vsync_requests = outstanding_vsync_requests_;
  outstanding_vsync_requests_ = 0;
  RequestVSyncUpdate(outstanding_vsync_requests);
}

void RenderWidgetHostViewAndroid::StopObservingRootWindow() {
  if (!content_view_core_window_android_) {
    DCHECK(!observing_root_window_);
    return;
  }

  if (!observing_root_window_)
    return;

  // Reset window state variables to their defaults.
  is_window_activity_started_ = true;
  is_window_visible_ = true;
  observing_root_window_ = false;
  content_view_core_window_android_->RemoveObserver(this);
}

void RenderWidgetHostViewAndroid::SendBeginFrame(base::TimeTicks frame_time,
                                                 base::TimeDelta vsync_period) {
  TRACE_EVENT1("cc", "RenderWidgetHostViewAndroid::SendBeginFrame",
               "frame_time_us", frame_time.ToInternalValue());

  if (using_browser_compositor_) {
    // TODO(brianderson): Replace this hardcoded deadline after Android
    // switches to Surfaces and the Browser's commit isn't in the critcal path.
    base::TimeTicks deadline = frame_time + (vsync_period * 0.6);

    host_->Send(new ViewMsg_BeginFrame(
        host_->GetRoutingID(),
        cc::BeginFrameArgs::Create(BEGINFRAME_FROM_HERE, frame_time, deadline,
                                   vsync_period, cc::BeginFrameArgs::NORMAL)));
  } else if (sync_compositor_) {
    // The synchronous compositor synchronously does it's work in this call.
    // It does not use a deadline.
    sync_compositor_->BeginFrame(cc::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, frame_time, base::TimeTicks(), vsync_period,
        cc::BeginFrameArgs::NORMAL));
  }
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

void RenderWidgetHostViewAndroid::RequestDisallowInterceptTouchEvent() {
  if (content_view_core_)
    content_view_core_->RequestDisallowInterceptTouchEvent();
}

void RenderWidgetHostViewAndroid::EvictDelegatedFrame() {
  if (layer_.get())
    DestroyDelegatedContent();
  frame_evictor_->DiscardedFrame();
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

bool RenderWidgetHostViewAndroid::GetScreenColorProfile(
    std::vector<char>* color_profile) {
  DCHECK(color_profile->empty());
  NOTREACHED();
  return false;
}

// TODO(jrg): Find out the implications and answer correctly here,
// as we are returning the WebView and not root window bounds.
gfx::Rect RenderWidgetHostViewAndroid::GetBoundsInRootWindow() {
  return GetViewBounds();
}

void RenderWidgetHostViewAndroid::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch, InputEventAckState ack_result) {
  const bool event_consumed = ack_result == INPUT_EVENT_ACK_STATE_CONSUMED;
  gesture_provider_.OnTouchEventAck(touch.event.uniqueTouchEventId,
                                    event_consumed);
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
  if (selection_controller_ &&
      blink::WebInputEvent::isGestureEventType(input_event.type)) {
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    switch (gesture_event.type) {
      case blink::WebInputEvent::GestureLongPress:
        if (selection_controller_->WillHandleLongPressEvent(
                base::TimeTicks() +
                    base::TimeDelta::FromSecondsD(input_event.timeStampSeconds),
                gfx::PointF(gesture_event.x, gesture_event.y))) {
          return INPUT_EVENT_ACK_STATE_CONSUMED;
        }
        break;

      case blink::WebInputEvent::GestureTap:
        if (selection_controller_->WillHandleTapEvent(
                gfx::PointF(gesture_event.x, gesture_event.y),
                gesture_event.data.tap.tapCount)) {
          return INPUT_EVENT_ACK_STATE_CONSUMED;
        }
        break;

      case blink::WebInputEvent::GestureScrollBegin:
        selection_controller_->OnScrollBeginEvent();
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

  if (content_view_core_ && content_view_core_->FilterInputEvent(input_event))
    return INPUT_EVENT_ACK_STATE_CONSUMED;

  if (!host_)
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;

  if (input_event.type == blink::WebInputEvent::GestureTapDown ||
      input_event.type == blink::WebInputEvent::TouchStart) {
    GpuDataManagerImpl* gpu_data = GpuDataManagerImpl::GetInstance();
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim && gpu_data &&
        gpu_data->IsDriverBugWorkaroundActive(gpu::WAKE_UP_GPU_BEFORE_DRAWING))
      shim->Send(new GpuMsg_WakeUpGpu);
  }

  if (sync_compositor_)
    return sync_compositor_->HandleInputEvent(input_event);
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
  if (!host_)
    return;

  RenderWidgetHostImpl* target_host = host_;

  // If there are multiple widgets on the page (such as when there are
  // out-of-process iframes), pick the one that should process this event.
  if (host_->delegate())
    target_host = host_->delegate()->GetFocusedRenderWidgetHost(host_);
  if (!target_host)
    return;

  target_host->ForwardKeyboardEvent(event);
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
    host_->ForwardGestureEventWithLatencyInfo(event, ui::LatencyInfo());
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

uint32_t RenderWidgetHostViewAndroid::GetSurfaceIdNamespace() {
  if (id_allocator_)
    return id_allocator_->id_namespace();
  return 0;
}

void RenderWidgetHostViewAndroid::SetContentViewCore(
    ContentViewCoreImpl* content_view_core) {
  DCHECK(!content_view_core || !content_view_core_ ||
         (content_view_core_ == content_view_core));
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
  content_view_core_window_android_ =
      content_view_core_ ? content_view_core_->GetWindowAndroid() : nullptr;
  DCHECK_EQ(!!content_view_core_, !!content_view_core_window_android_);

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
  if (!content_view_core_) {
    sync_compositor_.reset();
    return;
  }

  if (is_showing_)
    StartObservingRootWindow();

  if (resize)
    WasResized();

  if (!selection_controller_)
    selection_controller_ = CreateSelectionController(this, content_view_core_);

  if (!overscroll_controller_ &&
      content_view_core_window_android_->GetCompositor()) {
    overscroll_controller_ = CreateOverscrollController(content_view_core_);
  }

  if (!sync_compositor_) {
    sync_compositor_ = SynchronousCompositorBase::Create(
        this, content_view_core_->GetWebContents());
  }
  if (sync_compositor_)
    sync_compositor_->DidBecomeCurrent();
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
      ui::CreateWebGestureEventFromGestureEventData(gesture);
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

void RenderWidgetHostViewAndroid::OnRootWindowVisibilityChanged(bool visible) {
  TRACE_EVENT1("browser",
               "RenderWidgetHostViewAndroid::OnRootWindowVisibilityChanged",
               "visible", visible);
  DCHECK(observing_root_window_);
  if (is_window_visible_ == visible)
    return;

  is_window_visible_ = visible;

  if (visible)
    ShowInternal();
  else
    HideInternal();
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
  if (!host_ || host_->is_hidden())
    return;

  if (outstanding_vsync_requests_ & FLUSH_INPUT) {
    outstanding_vsync_requests_ &= ~FLUSH_INPUT;
    host_->FlushInput();
  }

  if (outstanding_vsync_requests_ & BEGIN_FRAME ||
      outstanding_vsync_requests_ & PERSISTENT_BEGIN_FRAME) {
    outstanding_vsync_requests_ &= ~BEGIN_FRAME;
    SendBeginFrame(frame_time, vsync_period);
  }

  // This allows for SendBeginFrame and FlushInput to modify
  // outstanding_vsync_requests.
  uint32_t outstanding_vsync_requests = outstanding_vsync_requests_;
  outstanding_vsync_requests_ = 0;
  RequestVSyncUpdate(outstanding_vsync_requests);
}

void RenderWidgetHostViewAndroid::OnAnimate(base::TimeTicks begin_frame_time) {
  if (Animate(begin_frame_time))
    SetNeedsAnimate();
}

void RenderWidgetHostViewAndroid::OnActivityStopped() {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAndroid::OnActivityStopped");
  DCHECK(observing_root_window_);
  is_window_activity_started_ = false;
  HideInternal();
}

void RenderWidgetHostViewAndroid::OnActivityStarted() {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAndroid::OnActivityStarted");
  DCHECK(observing_root_window_);
  is_window_activity_started_ = true;
  ShowInternal();
}

void RenderWidgetHostViewAndroid::OnLostResources() {
  ReleaseLocksOnSurface();
  if (layer_.get())
    DestroyDelegatedContent();
  DCHECK(ack_callbacks_.empty());
}

// static
void RenderWidgetHostViewAndroid::
    PrepareTextureCopyOutputResultForDelegatedReadback(
        const gfx::Size& dst_size_in_pixel,
        SkColorType color_type,
        const base::TimeTicks& start_time,
        scoped_refptr<cc::Layer> readback_layer,
        const ReadbackRequestCallback& callback,
        scoped_ptr<cc::CopyOutputResult> result) {
  readback_layer->RemoveFromParent();
  PrepareTextureCopyOutputResult(dst_size_in_pixel, color_type, start_time,
                                 callback, std::move(result));
}

// TODO(wjmaclean): There is significant overlap between
// PrepareTextureCopyOutputResult and CopyFromCompositingSurfaceFinished in
// this file, and the versions in surface_utils.cc. They should
// be merged. See https://crbug.com/582955

// static
void RenderWidgetHostViewAndroid::PrepareTextureCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    SkColorType color_type,
    const base::TimeTicks& start_time,
    const ReadbackRequestCallback& callback,
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

  GLHelper* gl_helper = GetPostReadbackGLHelper();
  if (!gl_helper)
    return;
  if (!gl_helper->IsReadbackConfigSupported(color_type))
    color_type = kRGBA_8888_SkColorType;
  scoped_ptr<SkBitmap> bitmap(new SkBitmap);
  if (!bitmap->tryAllocPixels(SkImageInfo::Make(output_size_in_pixel.width(),
                                                output_size_in_pixel.height(),
                                                color_type,
                                                kOpaque_SkAlphaType))) {
    scoped_callback_runner.Reset(
        base::Bind(callback, SkBitmap(), READBACK_BITMAP_ALLOCATION_FAILURE));
    return;
  }


  scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock(
      new SkAutoLockPixels(*bitmap));
  uint8_t* pixels = static_cast<uint8_t*>(bitmap->getPixels());

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());
  if (!texture_mailbox.IsTexture())
    return;

  ignore_result(scoped_callback_runner.Release());

  gl_helper->CropScaleReadbackAndCleanMailbox(
      texture_mailbox.mailbox(), texture_mailbox.sync_token(), result->size(),
      gfx::Rect(result->size()), output_size_in_pixel, pixels, color_type,
      base::Bind(&CopyFromCompositingSurfaceFinished, callback,
                 base::Passed(&release_callback), base::Passed(&bitmap),
                 start_time, base::Passed(&bitmap_pixels_lock)),
      GLHelper::SCALER_QUALITY_GOOD);
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
  const gfx::Display& display = gfx::Screen::GetScreen()->GetPrimaryDisplay();
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

}  // namespace content
