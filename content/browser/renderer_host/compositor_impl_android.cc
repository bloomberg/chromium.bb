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
#include "cc/raster/single_thread_task_graph_runner.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/common/gl_helper.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/texture_mailbox_deleter.h"
#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator_android.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/direct_layer_tree_frame_sink.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu_stream_constants.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/compositor_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "gpu/vulkan/features.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "ui/android/window_android.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_utils.h"

namespace gpu {
struct GpuProcessHostedCALayerTreeParamsMac;
}

namespace content {

namespace {

// The client_id used here should not conflict with the client_id generated
// from RenderWidgetHostImpl.
constexpr uint32_t kDefaultClientId = 0u;

class SingleThreadTaskGraphRunner : public cc::SingleThreadTaskGraphRunner {
 public:
  SingleThreadTaskGraphRunner() {
    Start("CompositorTileWorker1", base::SimpleThread::Options());
  }

  ~SingleThreadTaskGraphRunner() override { Shutdown(); }
};

struct CompositorDependencies {
  CompositorDependencies() : frame_sink_id_allocator(kDefaultClientId) {
    // TODO(kylechar): Switch this back to kDisableSurfaceReferences.
    auto surface_lifetime_type =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            "enable-surface-references")
            ? viz::SurfaceManager::LifetimeType::REFERENCES
            : viz::SurfaceManager::LifetimeType::SEQUENCES;

    // TODO(danakj): Don't make a FrameSinkManagerImpl when display is in the
    // Gpu process, instead get the mojo pointer from the Gpu process.
    frame_sink_manager_impl =
        std::make_unique<viz::FrameSinkManagerImpl>(surface_lifetime_type);
    surface_utils::ConnectWithLocalFrameSinkManager(
        &host_frame_sink_manager, frame_sink_manager_impl.get());
  }

  SingleThreadTaskGraphRunner task_graph_runner;
  viz::HostFrameSinkManager host_frame_sink_manager;
  viz::FrameSinkIdAllocator frame_sink_id_allocator;
  // This is owned here so that SurfaceManager will be accessible in process
  // when display is in the same process. Other than using SurfaceManager,
  // access to |in_process_frame_sink_manager_| should happen via
  // |host_frame_sink_manager_| instead which uses Mojo. See
  // http://crbug.com/657959.
  std::unique_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_impl;

#if BUILDFLAG(ENABLE_VULKAN)
  scoped_refptr<viz::VulkanContextProvider> vulkan_context_provider;
#endif
};

base::LazyInstance<CompositorDependencies>::DestructorAtExit
    g_compositor_dependencies = LAZY_INSTANCE_INITIALIZER;

const unsigned int kMaxDisplaySwapBuffers = 1U;

#if BUILDFLAG(ENABLE_VULKAN)
scoped_refptr<viz::VulkanContextProvider> GetSharedVulkanContextProvider() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableVulkan)) {
    scoped_refptr<viz::VulkanContextProvider> context_provider =
        g_compositor_dependencies.Get().vulkan_context_provider;
    if (!*context_provider)
      *context_provider = viz::VulkanInProcessContextProvider::Create();
    return *context_provider;
  }
  return nullptr;
}
#endif

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
    const gfx::ColorSpace& display_color_space,
    bool requires_alpha_channel) {
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
  if (display_color_space == gfx::ColorSpace::CreateSRGB()) {
    attributes.color_space = gpu::gles2::COLOR_SPACE_SRGB;
  } else if (display_color_space == gfx::ColorSpace::CreateDisplayP3D65()) {
    attributes.color_space = gpu::gles2::COLOR_SPACE_DISPLAY_P3;
  } else {
    attributes.color_space = gpu::gles2::COLOR_SPACE_UNSPECIFIED;
    DLOG(ERROR) << "Android color space is neither sRGB nor P3, output color "
                   "will be incorrect.";
  }

  if (requires_alpha_channel) {
    attributes.alpha_size = 8;
  } else if (base::SysInfo::AmountOfPhysicalMemoryMB() <= 512) {
    // In this case we prefer to use RGB565 format instead of RGBA8888 if
    // possible.
    // TODO(danakj): GpuCommandBufferStub constructor checks for alpha == 0 in
    // order to enable 565, but it should avoid using 565 when -1s are
    // specified
    // (IOW check that a <= 0 && rgb > 0 && rgb <= 565) then alpha should be
    // -1.
    // TODO(liberato): This condition is memorized in ComositorView.java, to
    // avoid using two surfaces temporarily during alpha <-> no alpha
    // transitions.  If these mismatch, then we risk a power regression if the
    // SurfaceView is not marked as eOpaque (FORMAT_OPAQUE), and we have an
    // EGL surface with an alpha channel.  SurfaceFlinger needs at least one of
    // those hints to optimize out alpha blending.
    attributes.alpha_size = 0;
    attributes.red_size = 5;
    attributes.green_size = 6;
    attributes.blue_size = 5;
  }

  return attributes;
}

void CreateContextProviderAfterGpuChannelEstablished(
    gpu::SurfaceHandle handle,
    gpu::gles2::ContextCreationAttribHelper attributes,
    gpu::SharedMemoryLimits shared_memory_limits,
    Compositor::ContextProviderCallback callback,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  if (!gpu_channel_host)
    callback.Run(nullptr);

  int32_t stream_id = kGpuStreamIdDefault;
  gpu::SchedulingPriority stream_priority = kGpuStreamPriorityUI;

  constexpr bool automatic_flushes = false;
  constexpr bool support_locking = false;

  scoped_refptr<ui::ContextProviderCommandBuffer> context_provider =
      new ui::ContextProviderCommandBuffer(
          std::move(gpu_channel_host), stream_id, stream_priority, handle,
          GURL(std::string("chrome://gpu/Compositor::CreateContextProvider")),
          automatic_flushes, support_locking, shared_memory_limits, attributes,
          nullptr /* shared_context */,
          ui::command_buffer_metrics::CONTEXT_TYPE_UNKNOWN);
  callback.Run(std::move(context_provider));
}

class AndroidOutputSurface : public viz::OutputSurface {
 public:
  AndroidOutputSurface(
      scoped_refptr<ui::ContextProviderCommandBuffer> context_provider,
      base::Closure swap_buffers_callback)
      : viz::OutputSurface(std::move(context_provider)),
        swap_buffers_callback_(std::move(swap_buffers_callback)),
        overlay_candidate_validator_(
            new viz::CompositorOverlayCandidateValidatorAndroid()),
        weak_ptr_factory_(this) {
    capabilities_.max_frames_pending = kMaxDisplaySwapBuffers;
  }

  ~AndroidOutputSurface() override = default;

  void SwapBuffers(viz::OutputSurfaceFrame frame) override {
    GetCommandBufferProxy()->AddLatencyInfo(frame.latency_info);
    if (frame.sub_buffer_rect) {
      DCHECK(frame.sub_buffer_rect->IsEmpty());
      context_provider_->ContextSupport()->CommitOverlayPlanes();
    } else {
      context_provider_->ContextSupport()->Swap();
    }
  }

  void BindToClient(viz::OutputSurfaceClient* client) override {
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

  void SetDrawRectangle(const gfx::Rect& rect) override {}

  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override {
    context_provider()->ContextGL()->ResizeCHROMIUM(
        size.width(), size.height(), device_scale_factor,
        gl::GetGLColorSpace(color_space), has_alpha);
  }

  viz::OverlayCandidateValidator* GetOverlayCandidateValidator()
      const override {
    return overlay_candidate_validator_.get();
  }

  bool IsDisplayedAsOverlayPlane() const override { return false; }
  unsigned GetOverlayTextureId() const override { return 0; }
  gfx::BufferFormat GetOverlayBufferFormat() const override {
    return gfx::BufferFormat::RGBX_8888;
  }
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
    RenderWidgetHostImpl::OnGpuSwapBuffersCompleted(latency_info);
    client_->DidReceiveSwapBuffersAck();
    swap_buffers_callback_.Run();
  }

 private:
  viz::OutputSurfaceClient* client_ = nullptr;
  base::Closure swap_buffers_callback_;
  std::unique_ptr<viz::OverlayCandidateValidator> overlay_candidate_validator_;
  base::WeakPtrFactory<AndroidOutputSurface> weak_ptr_factory_;
};

#if BUILDFLAG(ENABLE_VULKAN)
class VulkanOutputSurface : public viz::OutputSurface {
 public:
  explicit VulkanOutputSurface(
      scoped_refptr<viz::VulkanContextProvider> vulkan_context_provider,
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

  bool BindToClient(viz::OutputSurfaceClient* client) override {
    if (!OutputSurface::BindToClient(client))
      return false;
    return true;
  }

  void SwapBuffers(viz::CompositorFrame frame) override {
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
void Compositor::CreateContextProvider(
    gpu::SurfaceHandle handle,
    gpu::gles2::ContextCreationAttribHelper attributes,
    gpu::SharedMemoryLimits shared_memory_limits,
    ContextProviderCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserMainLoop::GetInstance()
      ->gpu_channel_establish_factory()
      ->EstablishGpuChannel(
          base::Bind(&CreateContextProviderAfterGpuChannelEstablished, handle,
                     attributes, shared_memory_limits, callback));
}

// static
viz::FrameSinkManagerImpl* CompositorImpl::GetFrameSinkManager() {
  return g_compositor_dependencies.Get().frame_sink_manager_impl.get();
}

// static
viz::HostFrameSinkManager* CompositorImpl::GetHostFrameSinkManager() {
  return &g_compositor_dependencies.Get().host_frame_sink_manager;
}

// static
viz::FrameSinkId CompositorImpl::AllocateFrameSinkId() {
  return g_compositor_dependencies.Get()
      .frame_sink_id_allocator.NextFrameSinkId();
}

// static
bool CompositorImpl::IsInitialized() {
  return g_initialized;
}

CompositorImpl::CompositorImpl(CompositorClient* client,
                               gfx::NativeWindow root_window)
    : frame_sink_id_(AllocateFrameSinkId()),
      resource_manager_(root_window),
      window_(NULL),
      surface_handle_(gpu::kNullSurfaceHandle),
      client_(client),
      root_window_(root_window),
      needs_animate_(false),
      pending_frames_(0U),
      num_successive_context_creation_failures_(0),
      layer_tree_frame_sink_request_pending_(false),
      weak_factory_(this) {
  GetHostFrameSinkManager()->RegisterFrameSinkId(frame_sink_id_, this);
#if DCHECK_IS_ON()
  GetHostFrameSinkManager()->SetFrameSinkDebugLabel(frame_sink_id_,
                                                    "CompositorImpl");
#endif
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
  GetHostFrameSinkManager()->InvalidateFrameSinkId(frame_sink_id_);
}

bool CompositorImpl::IsForSubframe() {
  return false;
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
    // Register first, SetVisible() might create a LayerTreeFrameSink.
    surface_handle_ = tracker->AddSurfaceForNativeWidget(
        gpu::GpuSurfaceTracker::SurfaceRecord(window, surface));
    SetVisible(true);
    ANativeWindow_release(window);
  }
}

void CompositorImpl::CreateLayerTreeHost() {
  DCHECK(!host_);

  cc::LayerTreeSettings settings;
  settings.use_zero_copy = true;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  settings.initial_debug_state.SetRecordRenderingStats(
      command_line->HasSwitch(cc::switches::kEnableGpuBenchmarking));
  settings.initial_debug_state.show_fps_counter =
      command_line->HasSwitch(cc::switches::kUIShowFPSCounter);
  settings.single_thread_proxy_scheduler = true;
  settings.resource_settings.buffer_to_texture_target_map =
      CreateBufferToTextureTargetMap();

  animation_host_ = cc::AnimationHost::CreateMainInstance();

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.task_graph_runner = &g_compositor_dependencies.Get().task_graph_runner;
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.settings = &settings;
  params.mutator_host = animation_host_.get();
  host_ = cc::LayerTreeHost::CreateSingleThreaded(this, &params);
  DCHECK(!host_->IsVisible());
  host_->SetRootLayer(root_window_->GetLayer());
  host_->SetFrameSinkId(frame_sink_id_);
  host_->SetViewportSize(size_);
  host_->SetDeviceScaleFactor(1);

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
    host_->ReleaseLayerTreeFrameSink();
    has_layer_tree_frame_sink_ = false;
    pending_frames_ = 0;
    if (display_) {
      GetFrameSinkManager()->UnregisterBeginFrameSource(
          root_window_->GetBeginFrameSource());
    }
    display_.reset();
  } else {
    host_->SetVisible(true);
    if (layer_tree_frame_sink_request_pending_)
      HandlePendingLayerTreeFrameSinkRequest();
  }
}

void CompositorImpl::SetWindowBounds(const gfx::Size& size) {
  if (size_ == size)
    return;

  size_ = size;
  if (host_)
    host_->SetViewportSize(size);
  if (display_)
    display_->Resize(size);
  root_window_->GetLayer()->SetBounds(size);
}

void CompositorImpl::SetRequiresAlphaChannel(bool flag) {
  requires_alpha_channel_ = flag;
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

void CompositorImpl::RequestNewLayerTreeFrameSink() {
  DCHECK(!layer_tree_frame_sink_request_pending_)
      << "LayerTreeFrameSink request is already pending?";

  layer_tree_frame_sink_request_pending_ = true;
  HandlePendingLayerTreeFrameSinkRequest();
}

void CompositorImpl::DidInitializeLayerTreeFrameSink() {
  layer_tree_frame_sink_request_pending_ = false;
  has_layer_tree_frame_sink_ = true;
  for (auto& frame_sink_id : pending_child_frame_sink_ids_)
    AddChildFrameSink(frame_sink_id);

  pending_child_frame_sink_ids_.clear();
}

void CompositorImpl::DidFailToInitializeLayerTreeFrameSink() {
  // The context is bound/initialized before handing it to the
  // LayerTreeFrameSink.
  NOTREACHED();
}

void CompositorImpl::HandlePendingLayerTreeFrameSinkRequest() {
  DCHECK(layer_tree_frame_sink_request_pending_);

  // We might have been made invisible now.
  if (!host_->IsVisible())
    return;

#if BUILDFLAG(ENABLE_VULKAN)
  CreateVulkanOutputSurface()
  if (display_)
    return;
#endif

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableTimeoutsForProfiling)) {
#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(SYZYASAN) || defined(CYGPROFILE_INSTRUMENTATION)
    const int64_t kGpuChannelTimeoutInSeconds = 40;
#else
    // The GPU watchdog timeout is 15 seconds (1.5x the kGpuTimeout value due to
    // logic in GpuWatchdogThread). Make this slightly longer to give the GPU a
    // chance to crash itself before crashing the browser.
    const int64_t kGpuChannelTimeoutInSeconds = 20;
#endif

    // Start the timer first, if the result comes synchronously, we want it to
    // stop in the callback.
    establish_gpu_channel_timeout_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kGpuChannelTimeoutInSeconds),
        this, &CompositorImpl::OnGpuChannelTimeout);
  }

  DCHECK(surface_handle_ != gpu::kNullSurfaceHandle);
  BrowserMainLoop::GetInstance()
      ->gpu_channel_establish_factory()
      ->EstablishGpuChannel(base::Bind(&CompositorImpl::OnGpuChannelEstablished,
                                       weak_factory_.GetWeakPtr()));
}

void CompositorImpl::OnGpuChannelTimeout() {
  LOG(FATAL) << "Timed out waiting for GPU channel.";
}

#if BUILDFLAG(ENABLE_VULKAN)
void CompositorImpl::CreateVulkanOutputSurface() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableVulkan))
    return;

  scoped_refptr<viz::VulkanContextProvider> vulkan_context_provider =
      GetSharedVulkanContextProvider();
  if (!vulkan_context_provider)
    return;

  // TODO(crbug.com/582558): Need to match GL and implement DidSwapBuffers.
  auto vulkan_surface = base::MakeUnique<VulkanOutputSurface>(
      vulkan_context_provider, base::ThreadTaskRunnerHandle::Get());
  if (!vulkan_surface->Initialize(window_))
    return;

  InitializeDisplay(std::move(vulkan_surface),
                    std::move(vulkan_context_provider), nullptr);
}
#endif

void CompositorImpl::OnGpuChannelEstablished(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  establish_gpu_channel_timeout_.Stop();

  // We might end up queing multiple GpuChannel requests for the same
  // LayerTreeFrameSink request as the visibility of the compositor changes, so
  // the LayerTreeFrameSink request could have been handled already.
  if (!layer_tree_frame_sink_request_pending_)
    return;

  if (!gpu_channel_host) {
    HandlePendingLayerTreeFrameSinkRequest();
    return;
  }

  // We don't need the context anymore if we are invisible.
  if (!host_->IsVisible())
    return;

  DCHECK(window_);
  DCHECK_NE(surface_handle_, gpu::kNullSurfaceHandle);

  int32_t stream_id = kGpuStreamIdDefault;
  gpu::SchedulingPriority stream_priority = kGpuStreamPriorityUI;

  constexpr bool support_locking = false;
  constexpr bool automatic_flushes = false;
  display_color_space_ = display::Screen::GetScreen()
                             ->GetDisplayNearestWindow(root_window_)
                             .color_space();

  ui::ContextProviderCommandBuffer* shared_context = nullptr;
  scoped_refptr<ui::ContextProviderCommandBuffer> context_provider =
      new ui::ContextProviderCommandBuffer(
          std::move(gpu_channel_host), stream_id, stream_priority,
          surface_handle_,
          GURL(std::string("chrome://gpu/CompositorImpl::") +
               std::string("CompositorContextProvider")),
          automatic_flushes, support_locking,
          GetCompositorContextSharedMemoryLimits(root_window_),
          GetCompositorContextAttributes(display_color_space_,
                                         requires_alpha_channel_),
          shared_context,
          ui::command_buffer_metrics::DISPLAY_COMPOSITOR_ONSCREEN_CONTEXT);
  auto result = context_provider->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess) {
    // TODO(danakj): Give up on fatal error instead of after 2 tries.
    LOG(ERROR) << "Failed to init viz::ContextProvider for compositor.";
    LOG_IF(FATAL, ++num_successive_context_creation_failures_ >= 2)
        << "Too many context creation failures. Giving up... ";
    HandlePendingLayerTreeFrameSinkRequest();
    return;
  }

  // Unretained is safe this owns viz::Display which owns OutputSurface.
  auto display_output_surface = base::MakeUnique<AndroidOutputSurface>(
      context_provider,
      base::Bind(&CompositorImpl::DidSwapBuffers, base::Unretained(this)));
  InitializeDisplay(std::move(display_output_surface), nullptr,
                    std::move(context_provider));
}

void CompositorImpl::InitializeDisplay(
    std::unique_ptr<viz::OutputSurface> display_output_surface,
    scoped_refptr<viz::VulkanContextProvider> vulkan_context_provider,
    scoped_refptr<viz::ContextProvider> context_provider) {
  DCHECK(layer_tree_frame_sink_request_pending_);

  pending_frames_ = 0;
  num_successive_context_creation_failures_ = 0;

  if (context_provider) {
    gpu_capabilities_ = context_provider->ContextCapabilities();
  } else {
    // TODO(danakj): Populate gpu_capabilities_ for VulkanContextProvider.
  }

  viz::FrameSinkManagerImpl* manager = GetFrameSinkManager();
  auto* task_runner = base::ThreadTaskRunnerHandle::Get().get();
  auto scheduler = base::MakeUnique<viz::DisplayScheduler>(
      root_window_->GetBeginFrameSource(), task_runner,
      display_output_surface->capabilities().max_frames_pending);

  viz::RendererSettings renderer_settings;
  renderer_settings.allow_antialiasing = false;
  renderer_settings.highp_threshold_min = 2048;
  auto* gpu_memory_buffer_manager = BrowserMainLoop::GetInstance()
                                        ->gpu_channel_establish_factory()
                                        ->GetGpuMemoryBufferManager();
  display_ = base::MakeUnique<viz::Display>(
      viz::ServerSharedBitmapManager::current(), gpu_memory_buffer_manager,
      renderer_settings, frame_sink_id_, std::move(display_output_surface),
      std::move(scheduler),
      base::MakeUnique<viz::TextureMailboxDeleter>(task_runner));

  auto layer_tree_frame_sink =
      vulkan_context_provider
          ? base::MakeUnique<viz::DirectLayerTreeFrameSink>(
                frame_sink_id_, GetHostFrameSinkManager(), manager,
                display_.get(), vulkan_context_provider)
          : base::MakeUnique<viz::DirectLayerTreeFrameSink>(
                frame_sink_id_, GetHostFrameSinkManager(), manager,
                display_.get(), context_provider,
                nullptr /* worker_context_provider */,
                gpu_memory_buffer_manager,
                viz::ServerSharedBitmapManager::current());

  display_->SetVisible(true);
  display_->Resize(size_);
  display_->SetColorSpace(display_color_space_, display_color_space_);
  GetFrameSinkManager()->RegisterBeginFrameSource(
      root_window_->GetBeginFrameSource(), frame_sink_id_);
  host_->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
}

void CompositorImpl::DidSwapBuffers() {
  client_->DidSwapBuffers();
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
  pending_frames_++;
}

void CompositorImpl::DidReceiveCompositorFrameAck() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidReceiveCompositorFrameAck");
  DCHECK_GT(pending_frames_, 0U);
  pending_frames_--;
  client_->DidSwapFrame(pending_frames_);
}

void CompositorImpl::DidLoseLayerTreeFrameSink() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidLoseLayerTreeFrameSink");
  has_layer_tree_frame_sink_ = false;
  client_->DidSwapFrame(0);
}

void CompositorImpl::DidCommit() {
  root_window_->OnCompositingDidCommit();
}

void CompositorImpl::AttachLayerForReadback(scoped_refptr<cc::Layer> layer) {
  readback_layer_tree_->AddChild(layer);
}

void CompositorImpl::RequestCopyOfOutputOnRootLayer(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  root_window_->GetLayer()->RequestCopyOfOutput(std::move(request));
}

void CompositorImpl::SetNeedsAnimate() {
  needs_animate_ = true;
  if (!host_->IsVisible())
    return;

  TRACE_EVENT0("compositor", "Compositor::SetNeedsAnimate");
  host_->SetNeedsAnimate();
}

viz::FrameSinkId CompositorImpl::GetFrameSinkId() {
  return frame_sink_id_;
}

void CompositorImpl::AddChildFrameSink(const viz::FrameSinkId& frame_sink_id) {
  if (has_layer_tree_frame_sink_) {
    GetHostFrameSinkManager()->RegisterFrameSinkHierarchy(frame_sink_id_,
                                                          frame_sink_id);
  } else {
    pending_child_frame_sink_ids_.insert(frame_sink_id);
  }
}

void CompositorImpl::RemoveChildFrameSink(
    const viz::FrameSinkId& frame_sink_id) {
  auto it = pending_child_frame_sink_ids_.find(frame_sink_id);
  if (it != pending_child_frame_sink_ids_.end()) {
    pending_child_frame_sink_ids_.erase(it);
    return;
  }
  GetHostFrameSinkManager()->UnregisterFrameSinkHierarchy(frame_sink_id_,
                                                          frame_sink_id);
}

void CompositorImpl::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  // TODO(fsamuel): Once surface synchronization is turned on, the fallback
  // surface should be set here.
}

bool CompositorImpl::HavePendingReadbacks() {
  return !readback_layer_tree_->children().empty();
}

}  // namespace content
