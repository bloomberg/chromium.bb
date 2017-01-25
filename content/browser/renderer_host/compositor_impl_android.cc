// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_impl_android.h"

#include <android/bitmap.h>
#include <android/native_window_jni.h>
#include <stdint.h>
#include <unordered_set>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/sys_info.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/output_surface_frame.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/output/vulkan_in_process_context_provider.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/surfaces/direct_compositor_frame_sink.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/display_compositor/compositor_overlay_candidate_validator_android.h"
#include "components/display_compositor/gl_helper.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/context_provider_factory_impl_android.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/compositor_client.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "ui/android/window_android.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/swap_result.h"

namespace gpu {
struct GpuProcessHostedCALayerTreeParamsMac;
}

namespace content {

namespace {

const unsigned int kMaxDisplaySwapBuffers = 1U;

gpu::SharedMemoryLimits GetCompositorContextSharedMemoryLimits(
    gfx::NativeWindow window) {
  constexpr size_t kBytesPerPixel = 4;
  const gfx::Size size = display::Screen::GetScreen()
                             ->GetDisplayNearestWindow(window)
                             .GetSizeInPixel();
  const size_t full_screen_texture_size_in_bytes =
      size.width() * size.height() * kBytesPerPixel;

  gpu::SharedMemoryLimits limits;
  // This limit is meant to hold the contents of the display compositor
  // drawing the scene. See discussion here:
  // https://codereview.chromium.org/1900993002/diff/90001/content/browser/renderer_host/compositor_impl_android.cc?context=3&column_width=80&tab_spaces=8
  limits.command_buffer_size = 64 * 1024;
  // These limits are meant to hold the uploads for the browser UI without
  // any excess space.
  limits.start_transfer_buffer_size = 64 * 1024;
  limits.min_transfer_buffer_size = 64 * 1024;
  limits.max_transfer_buffer_size = full_screen_texture_size_in_bytes;
  // Texture uploads may use mapped memory so give a reasonable limit for
  // them.
  limits.mapped_memory_reclaim_limit = full_screen_texture_size_in_bytes;

  return limits;
}

gpu::gles2::ContextCreationAttribHelper GetCompositorContextAttributes(
    bool has_transparent_background) {
  // This is used for the browser compositor (offscreen) and for the display
  // compositor (onscreen), so ask for capabilities needed by either one.
  // The default framebuffer for an offscreen context is not used, so it does
  // not need alpha, stencil, depth, antialiasing. The display compositor does
  // not use these things either, except for alpha when it has a transparent
  // background.
  gpu::gles2::ContextCreationAttribHelper attributes;
  attributes.alpha_size = -1;
  attributes.stencil_size = 0;
  attributes.depth_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;

  if (has_transparent_background) {
    attributes.alpha_size = 8;
  } else if (base::SysInfo::IsLowEndDevice()) {
    // In this case we prefer to use RGB565 format instead of RGBA8888 if
    // possible.
    // TODO(danakj): GpuCommandBufferStub constructor checks for alpha == 0 in
    // order to enable 565, but it should avoid using 565 when -1s are
    // specified
    // (IOW check that a <= 0 && rgb > 0 && rgb <= 565) then alpha should be
    // -1.
    attributes.alpha_size = 0;
    attributes.red_size = 5;
    attributes.green_size = 6;
    attributes.blue_size = 5;
  }

  return attributes;
}

class AndroidOutputSurface : public cc::OutputSurface {
 public:
  explicit AndroidOutputSurface(
      scoped_refptr<ui::ContextProviderCommandBuffer> context_provider)
      : cc::OutputSurface(std::move(context_provider)),
        overlay_candidate_validator_(
            new display_compositor::
                CompositorOverlayCandidateValidatorAndroid()),
        weak_ptr_factory_(this) {
    capabilities_.max_frames_pending = kMaxDisplaySwapBuffers;
  }

  ~AndroidOutputSurface() override = default;

  void SwapBuffers(cc::OutputSurfaceFrame frame) override {
    GetCommandBufferProxy()->SetLatencyInfo(frame.latency_info);
    if (frame.sub_buffer_rect.IsEmpty()) {
      context_provider_->ContextSupport()->CommitOverlayPlanes();
    } else {
      DCHECK(frame.sub_buffer_rect == gfx::Rect(frame.size));
      context_provider_->ContextSupport()->Swap();
    }
  }

  void BindToClient(cc::OutputSurfaceClient* client) override {
    DCHECK(client);
    DCHECK(!client_);
    client_ = client;
    GetCommandBufferProxy()->SetSwapBuffersCompletionCallback(
        base::Bind(&AndroidOutputSurface::OnSwapBuffersCompleted,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void EnsureBackbuffer() override {}

  void DiscardBackbuffer() override {
    context_provider()->ContextGL()->DiscardBackbufferCHROMIUM();
  }

  void BindFramebuffer() override {
    context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override {
    context_provider()->ContextGL()->ResizeCHROMIUM(
        size.width(), size.height(), device_scale_factor, has_alpha);
  }

  cc::OverlayCandidateValidator* GetOverlayCandidateValidator() const override {
    return overlay_candidate_validator_.get();
  }

  bool IsDisplayedAsOverlayPlane() const override { return false; }
  unsigned GetOverlayTextureId() const override { return 0; }
  bool SurfaceIsSuspendForRecycle() const override { return false; }
  bool HasExternalStencilTest() const override { return false; }
  void ApplyExternalStencil() override {}

  uint32_t GetFramebufferCopyTextureFormat() override {
    auto* gl =
        static_cast<ui::ContextProviderCommandBuffer*>(context_provider());
    return gl->GetCopyTextureInternalFormat();
  }

 private:
  gpu::CommandBufferProxyImpl* GetCommandBufferProxy() {
    ui::ContextProviderCommandBuffer* provider_command_buffer =
        static_cast<ui::ContextProviderCommandBuffer*>(context_provider_.get());
    gpu::CommandBufferProxyImpl* command_buffer_proxy =
        provider_command_buffer->GetCommandBufferProxy();
    DCHECK(command_buffer_proxy);
    return command_buffer_proxy;
  }

  void OnSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) {
    RenderWidgetHostImpl::CompositorFrameDrawn(latency_info);
    client_->DidReceiveSwapBuffersAck();
  }

 private:
  cc::OutputSurfaceClient* client_ = nullptr;
  std::unique_ptr<cc::OverlayCandidateValidator> overlay_candidate_validator_;
  base::WeakPtrFactory<AndroidOutputSurface> weak_ptr_factory_;
};

#if defined(ENABLE_VULKAN)
class VulkanOutputSurface : public cc::OutputSurface {
 public:
  explicit VulkanOutputSurface(
      scoped_refptr<cc::VulkanContextProvider> vulkan_context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : OutputSurface(std::move(vulkan_context_provider)),
        task_runner_(std::move(task_runner)),
        weak_ptr_factory_(this) {}

  ~VulkanOutputSurface() override { Destroy(); }

  bool Initialize(gfx::AcceleratedWidget widget) {
    DCHECK(!surface_);
    std::unique_ptr<gpu::VulkanSurface> surface(
        gpu::VulkanSurface::CreateViewSurface(widget));
    if (!surface->Initialize(vulkan_context_provider()->GetDeviceQueue(),
                             gpu::VulkanSurface::DEFAULT_SURFACE_FORMAT)) {
      return false;
    }
    surface_ = std::move(surface);

    return true;
  }

  bool BindToClient(cc::OutputSurfaceClient* client) override {
    if (!OutputSurface::BindToClient(client))
      return false;
    return true;
  }

  void SwapBuffers(cc::CompositorFrame frame) override {
    surface_->SwapBuffers();
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&VulkanOutputSurface::SwapBuffersAck,
                                      weak_ptr_factory_.GetWeakPtr()));
  }

  void Destroy() {
    if (surface_) {
      surface_->Destroy();
      surface_.reset();
    }
  }

 private:
  void SwapBuffersAck() { client_->DidReceiveSwapBuffersAck(); }

  std::unique_ptr<gpu::VulkanSurface> surface_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<VulkanOutputSurface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VulkanOutputSurface);
};
#endif

static bool g_initialized = false;

class SingleThreadTaskGraphRunner : public cc::SingleThreadTaskGraphRunner {
 public:
  SingleThreadTaskGraphRunner() {
    Start("CompositorTileWorker1", base::SimpleThread::Options());
  }

  ~SingleThreadTaskGraphRunner() override {
    Shutdown();
  }
};

base::LazyInstance<SingleThreadTaskGraphRunner> g_task_graph_runner =
    LAZY_INSTANCE_INITIALIZER;

}  // anonymous namespace

// static
Compositor* Compositor::Create(CompositorClient* client,
                               gfx::NativeWindow root_window) {
  return client ? new CompositorImpl(client, root_window) : NULL;
}

// static
void Compositor::Initialize() {
  DCHECK(!CompositorImpl::IsInitialized());
  g_initialized = true;
}

// static
bool CompositorImpl::IsInitialized() {
  return g_initialized;
}

CompositorImpl::CompositorImpl(CompositorClient* client,
                               gfx::NativeWindow root_window)
    : frame_sink_id_(
          ui::ContextProviderFactory::GetInstance()->AllocateFrameSinkId()),
      resource_manager_(root_window),
      window_(NULL),
      surface_handle_(gpu::kNullSurfaceHandle),
      client_(client),
      root_window_(root_window),
      needs_animate_(false),
      pending_swapbuffers_(0U),
      num_successive_context_creation_failures_(0),
      compositor_frame_sink_request_pending_(false),
      weak_factory_(this) {
  ui::ContextProviderFactory::GetInstance()
      ->GetSurfaceManager()
      ->RegisterFrameSinkId(frame_sink_id_);
  DCHECK(client);
  DCHECK(root_window);
  DCHECK(root_window->GetLayer() == nullptr);
  root_window->SetLayer(cc::Layer::Create());
  readback_layer_tree_ = cc::Layer::Create();
  readback_layer_tree_->SetHideLayerAndSubtree(true);
  root_window->GetLayer()->AddChild(readback_layer_tree_);
  root_window->AttachCompositor(this);
  CreateLayerTreeHost();
  resource_manager_.Init(host_->GetUIResourceManager());
}

CompositorImpl::~CompositorImpl() {
  root_window_->DetachCompositor();
  root_window_->SetLayer(nullptr);
  // Clean-up any surface references.
  SetSurface(NULL);
  ui::ContextProviderFactory::GetInstance()
      ->GetSurfaceManager()
      ->InvalidateFrameSinkId(frame_sink_id_);
}

ui::UIResourceProvider& CompositorImpl::GetUIResourceProvider() {
  return *this;
}

ui::ResourceManager& CompositorImpl::GetResourceManager() {
  return resource_manager_;
}

void CompositorImpl::SetRootLayer(scoped_refptr<cc::Layer> root_layer) {
  if (subroot_layer_.get()) {
    subroot_layer_->RemoveFromParent();
    subroot_layer_ = NULL;
  }
  if (root_window_->GetLayer()) {
    subroot_layer_ = root_window_->GetLayer();
    root_window_->GetLayer()->AddChild(root_layer);
  }
}

void CompositorImpl::SetSurface(jobject surface) {
  JNIEnv* env = base::android::AttachCurrentThread();
  gpu::GpuSurfaceTracker* tracker = gpu::GpuSurfaceTracker::Get();

  if (window_) {
    // Shut down GL context before unregistering surface.
    SetVisible(false);
    tracker->RemoveSurface(surface_handle_);
    ANativeWindow_release(window_);
    window_ = NULL;

    tracker->UnregisterViewSurface(surface_handle_);
    surface_handle_ = gpu::kNullSurfaceHandle;
  }

  ANativeWindow* window = NULL;
  if (surface) {
    // Note: This ensures that any local references used by
    // ANativeWindow_fromSurface are released immediately. This is needed as a
    // workaround for https://code.google.com/p/android/issues/detail?id=68174
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    window = ANativeWindow_fromSurface(env, surface);
  }

  if (window) {
    window_ = window;
    ANativeWindow_acquire(window);
    surface_handle_ = tracker->AddSurfaceForNativeWidget(window);
    // Register first, SetVisible() might create a CompositorFrameSink.
    tracker->RegisterViewSurface(surface_handle_, surface);
    SetVisible(true);
    ANativeWindow_release(window);
  }
}

void CompositorImpl::CreateLayerTreeHost() {
  DCHECK(!host_);

  cc::LayerTreeSettings settings;
  settings.renderer_settings.refresh_rate = 60.0;
  settings.renderer_settings.allow_antialiasing = false;
  settings.renderer_settings.highp_threshold_min = 2048;
  settings.use_zero_copy = true;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  settings.initial_debug_state.SetRecordRenderingStats(
      command_line->HasSwitch(cc::switches::kEnableGpuBenchmarking));
  settings.initial_debug_state.show_fps_counter =
      command_line->HasSwitch(cc::switches::kUIShowFPSCounter);
  settings.single_thread_proxy_scheduler = true;

  animation_host_ = cc::AnimationHost::CreateMainInstance();

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.task_graph_runner = g_task_graph_runner.Pointer();
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.settings = &settings;
  params.mutator_host = animation_host_.get();
  host_ = cc::LayerTreeHost::CreateSingleThreaded(this, &params);
  DCHECK(!host_->IsVisible());
  host_->GetLayerTree()->SetRootLayer(root_window_->GetLayer());
  host_->SetFrameSinkId(frame_sink_id_);
  host_->GetLayerTree()->SetViewportSize(size_);
  SetHasTransparentBackground(false);
  host_->GetLayerTree()->SetDeviceScaleFactor(1);

  if (needs_animate_)
    host_->SetNeedsAnimate();
}

void CompositorImpl::SetVisible(bool visible) {
  TRACE_EVENT1("cc", "CompositorImpl::SetVisible", "visible", visible);
  if (!visible) {
    DCHECK(host_->IsVisible());

    // Make a best effort to try to complete pending readbacks.
    // TODO(crbug.com/637035): Consider doing this in a better way,
    // ideally with the guarantee of readbacks completing.
    if (display_.get() && HavePendingReadbacks())
      display_->ForceImmediateDrawAndSwapIfPossible();

    host_->SetVisible(false);
    host_->ReleaseCompositorFrameSink();
    pending_swapbuffers_ = 0;
    display_.reset();
  } else {
    host_->SetVisible(true);
    if (compositor_frame_sink_request_pending_)
      HandlePendingCompositorFrameSinkRequest();
  }
}

void CompositorImpl::SetWindowBounds(const gfx::Size& size) {
  if (size_ == size)
    return;

  size_ = size;
  if (host_)
    host_->GetLayerTree()->SetViewportSize(size);
  if (display_)
    display_->Resize(size);
  root_window_->GetLayer()->SetBounds(size);
}

void CompositorImpl::SetHasTransparentBackground(bool transparent) {
  has_transparent_background_ = transparent;
  if (host_) {
    host_->GetLayerTree()->set_has_transparent_background(transparent);

    // Give a delay in setting the background color to avoid the color for
    // the normal mode (white) affecting the UI transition.
    base::ThreadTaskRunnerHandle::Get().get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CompositorImpl::SetBackgroundColor,
                   weak_factory_.GetWeakPtr(),
                   transparent ? SK_ColorBLACK : SK_ColorWHITE),
        base::TimeDelta::FromMilliseconds(500));
  }
}

void CompositorImpl::SetBackgroundColor(int color) {
  host_->GetLayerTree()->set_background_color(color);
}

void CompositorImpl::SetNeedsComposite() {
  if (!host_->IsVisible())
    return;
  TRACE_EVENT0("compositor", "Compositor::SetNeedsComposite");
  host_->SetNeedsAnimate();
}

void CompositorImpl::UpdateLayerTreeHost() {
  client_->UpdateLayerTreeHost();
  if (needs_animate_) {
    needs_animate_ = false;
    root_window_->Animate(base::TimeTicks::Now());
  }
}

void CompositorImpl::RequestNewCompositorFrameSink() {
  DCHECK(!compositor_frame_sink_request_pending_)
      << "Output Surface Request is already pending?";

  compositor_frame_sink_request_pending_ = true;
  HandlePendingCompositorFrameSinkRequest();
}

void CompositorImpl::DidInitializeCompositorFrameSink() {
  compositor_frame_sink_request_pending_ = false;
}

void CompositorImpl::DidFailToInitializeCompositorFrameSink() {
  // The context is bound/initialized before handing it to the
  // CompositorFrameSink.
  NOTREACHED();
}

void CompositorImpl::HandlePendingCompositorFrameSinkRequest() {
  DCHECK(compositor_frame_sink_request_pending_);

  // We might have been made invisible now.
  if (!host_->IsVisible())
    return;

#if defined(ENABLE_VULKAN)
  CreateVulkanOutputSurface()
  if (display_)
    return;
#endif

  DCHECK(surface_handle_ != gpu::kNullSurfaceHandle);
  ContextProviderFactoryImpl::GetInstance()->RequestGpuChannelHost(base::Bind(
      &CompositorImpl::OnGpuChannelEstablished, weak_factory_.GetWeakPtr()));
}

#if defined(ENABLE_VULKAN)
void CompositorImpl::CreateVulkanOutputSurface() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableVulkan))
    return;

  scoped_refptr<cc::VulkanContextProvider> vulkan_context_provider =
      ui::ContextProviderFactory::GetInstance()
          ->GetSharedVulkanContextProvider();
  if (!vulkan_context_provider)
    return;

  auto vulkan_surface = base::MakeUnique<VulkanOutputSurface>(
      vulkan_context_provider, base::ThreadTaskRunnerHandle::Get());
  if (!vulkan_surface->Initialize(window_))
    return;

  InitializeDisplay(std::move(vulkan_surface),
                    std::move(vulkan_context_provider), nullptr);
}
#endif

void CompositorImpl::OnGpuChannelEstablished(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    ui::ContextProviderFactory::GpuChannelHostResult result) {
  // We might end up queing multiple GpuChannel requests for the same
  // CompositorFrameSink request as the visibility of the compositor changes, so
  // the CompositorFrameSink request could have been handled already.
  if (!compositor_frame_sink_request_pending_)
    return;

  switch (result) {
    // Don't retry if we are shutting down.
    case ui::ContextProviderFactory::GpuChannelHostResult::
        FAILURE_FACTORY_SHUTDOWN:
      break;
    case ui::ContextProviderFactory::GpuChannelHostResult::
        FAILURE_GPU_PROCESS_INITIALIZATION_FAILED:
      HandlePendingCompositorFrameSinkRequest();
      break;
    case ui::ContextProviderFactory::GpuChannelHostResult::SUCCESS:
      // We don't need the context anymore if we are invisible.
      if (!host_->IsVisible())
        return;

      DCHECK(window_);
      DCHECK_NE(surface_handle_, gpu::kNullSurfaceHandle);
      scoped_refptr<cc::ContextProvider> context_provider =
          ContextProviderFactoryImpl::GetInstance()
              ->CreateDisplayContextProvider(
                  surface_handle_,
                  GetCompositorContextSharedMemoryLimits(root_window_),
                  GetCompositorContextAttributes(has_transparent_background_),
                  false /*support_locking*/, false /*automatic_flushes*/,
                  std::move(gpu_channel_host));
      if (!context_provider->BindToCurrentThread()) {
        LOG(ERROR) << "Failed to init ContextProvider for compositor.";
        LOG_IF(FATAL, ++num_successive_context_creation_failures_ >= 2)
            << "Too many context creation failures. Giving up... ";
        HandlePendingCompositorFrameSinkRequest();
        break;
      }

      scoped_refptr<ui::ContextProviderCommandBuffer>
          context_provider_command_buffer =
              static_cast<ui::ContextProviderCommandBuffer*>(
                  context_provider.get());
      auto display_output_surface = base::MakeUnique<AndroidOutputSurface>(
          std::move(context_provider_command_buffer));
      InitializeDisplay(std::move(display_output_surface), nullptr,
                        std::move(context_provider));
      break;
  }
}

void CompositorImpl::InitializeDisplay(
    std::unique_ptr<cc::OutputSurface> display_output_surface,
    scoped_refptr<cc::VulkanContextProvider> vulkan_context_provider,
    scoped_refptr<cc::ContextProvider> context_provider) {
  DCHECK(compositor_frame_sink_request_pending_);

  pending_swapbuffers_ = 0;
  num_successive_context_creation_failures_ = 0;

  if (context_provider) {
    gpu_capabilities_ = context_provider->ContextCapabilities();
  } else {
    // TODO(danakj): Populate gpu_capabilities_ for VulkanContextProvider.
  }

  cc::SurfaceManager* manager =
      ui::ContextProviderFactory::GetInstance()->GetSurfaceManager();
  auto* task_runner = base::ThreadTaskRunnerHandle::Get().get();
  std::unique_ptr<cc::DisplayScheduler> scheduler(new cc::DisplayScheduler(
      task_runner, display_output_surface->capabilities().max_frames_pending));

  display_.reset(new cc::Display(
      HostSharedBitmapManager::current(),
      BrowserGpuMemoryBufferManager::current(),
      host_->GetSettings().renderer_settings, frame_sink_id_,
      root_window_->GetBeginFrameSource(), std::move(display_output_surface),
      std::move(scheduler),
      base::MakeUnique<cc::TextureMailboxDeleter>(task_runner)));

  auto compositor_frame_sink =
      vulkan_context_provider
          ? base::MakeUnique<cc::DirectCompositorFrameSink>(
                frame_sink_id_, manager, display_.get(),
                vulkan_context_provider)
          : base::MakeUnique<cc::DirectCompositorFrameSink>(
                frame_sink_id_, manager, display_.get(), context_provider,
                nullptr, BrowserGpuMemoryBufferManager::current(),
                HostSharedBitmapManager::current());

  display_->SetVisible(true);
  display_->Resize(size_);
  host_->SetCompositorFrameSink(std::move(compositor_frame_sink));
}

cc::UIResourceId CompositorImpl::CreateUIResource(
    cc::UIResourceClient* client) {
  TRACE_EVENT0("compositor", "CompositorImpl::CreateUIResource");
  return host_->GetUIResourceManager()->CreateUIResource(client);
}

void CompositorImpl::DeleteUIResource(cc::UIResourceId resource_id) {
  TRACE_EVENT0("compositor", "CompositorImpl::DeleteUIResource");
  host_->GetUIResourceManager()->DeleteUIResource(resource_id);
}

bool CompositorImpl::SupportsETC1NonPowerOfTwo() const {
  return gpu_capabilities_.texture_format_etc1_npot;
}

void CompositorImpl::DidSubmitCompositorFrame() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidSubmitCompositorFrame");
  pending_swapbuffers_++;
}

void CompositorImpl::DidReceiveCompositorFrameAck() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidReceiveCompositorFrameAck");
  DCHECK_GT(pending_swapbuffers_, 0U);
  pending_swapbuffers_--;
  client_->OnSwapBuffersCompleted(pending_swapbuffers_);
}

void CompositorImpl::DidLoseCompositorFrameSink() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidLoseCompositorFrameSink");
  client_->OnSwapBuffersCompleted(0);
}

void CompositorImpl::DidCommit() {
  root_window_->OnCompositingDidCommit();
}

void CompositorImpl::AttachLayerForReadback(scoped_refptr<cc::Layer> layer) {
  readback_layer_tree_->AddChild(layer);
}

void CompositorImpl::RequestCopyOfOutputOnRootLayer(
    std::unique_ptr<cc::CopyOutputRequest> request) {
  root_window_->GetLayer()->RequestCopyOfOutput(std::move(request));
}

void CompositorImpl::SetNeedsAnimate() {
  needs_animate_ = true;
  if (!host_->IsVisible())
    return;

  TRACE_EVENT0("compositor", "Compositor::SetNeedsAnimate");
  host_->SetNeedsAnimate();
}

cc::FrameSinkId CompositorImpl::GetFrameSinkId() {
  return frame_sink_id_;
}

bool CompositorImpl::HavePendingReadbacks() {
  return !readback_layer_tree_->children().empty();
}

}  // namespace content
