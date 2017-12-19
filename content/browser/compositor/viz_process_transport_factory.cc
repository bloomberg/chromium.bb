// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/viz_process_transport_factory.h"

#include <utility>

#include "base/command_line.h"
#include "base/single_thread_task_runner.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include "components/viz/client/client_layer_tree_frame_sink.h"
#include "components/viz/client/client_shared_bitmap_manager.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/host/forwarding_compositing_mode_reporter_impl.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/host/renderer_settings_creation.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/common/gpu_stream_constants.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/compositor/reflector.h"

#if defined(OS_WIN)
#include "ui/gfx/win/rendering_window_manager.h"
#endif

namespace content {
namespace {

// The client id for the browser process. It must not conflict with any
// child process client id.
constexpr uint32_t kBrowserClientId = 0u;

scoped_refptr<ui::ContextProviderCommandBuffer> CreateContextProviderImpl(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    bool support_locking,
    bool support_gles2_interface,
    bool support_raster_interface,
    ui::ContextProviderCommandBuffer* shared_context_provider,
    ui::command_buffer_metrics::ContextType type) {
  constexpr bool kAutomaticFlushes = false;

  gpu::gles2::ContextCreationAttribHelper attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  attributes.buffer_preserved = false;
  attributes.enable_gles2_interface = support_gles2_interface;
  attributes.enable_raster_interface = support_raster_interface;

  GURL url("chrome://gpu/VizProcessTransportFactory::CreateContextProvider");
  return base::MakeRefCounted<ui::ContextProviderCommandBuffer>(
      std::move(gpu_channel_host), gpu_memory_buffer_manager,
      kGpuStreamIdDefault, kGpuStreamPriorityUI, gpu::kNullSurfaceHandle,
      std::move(url), kAutomaticFlushes, support_locking,
      gpu::SharedMemoryLimits(), attributes, shared_context_provider, type);
}

bool CheckContextLost(viz::ContextProvider* context_provider) {
  if (!context_provider)
    return false;

  auto status = context_provider->ContextGL()->GetGraphicsResetStatusKHR();
  return status != GL_NO_ERROR;
}

}  // namespace

VizProcessTransportFactory::VizProcessTransportFactory(
    gpu::GpuChannelEstablishFactory* gpu_channel_establish_factory,
    scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner,
    viz::ForwardingCompositingModeReporterImpl* forwarding_mode_reporter)
    : gpu_channel_establish_factory_(gpu_channel_establish_factory),
      resize_task_runner_(std::move(resize_task_runner)),
      forwarding_mode_reporter_(forwarding_mode_reporter),
      frame_sink_id_allocator_(kBrowserClientId),
      task_graph_runner_(std::make_unique<cc::SingleThreadTaskGraphRunner>()),
      renderer_settings_(viz::CreateRendererSettings(
          CreateBufferUsageAndFormatExceptionList())),
      compositing_mode_watcher_binding_(this),
      weak_ptr_factory_(this) {
  DCHECK(gpu_channel_establish_factory_);
  task_graph_runner_->Start("CompositorTileWorker1",
                            base::SimpleThread::Options());
  GetHostFrameSinkManager()->SetConnectionLostCallback(
      base::BindRepeating(&VizProcessTransportFactory::OnGpuProcessLost,
                          weak_ptr_factory_.GetWeakPtr()));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableGpu) ||
      command_line->HasSwitch(switches::kDisableGpuCompositing)) {
    CompositingModeFallbackToSoftware();
  } else {
    // Make |this| a CompositingModeWatcher for the |forwarding_mode_reporter_|.
    viz::mojom::CompositingModeWatcherPtr watcher_ptr;
    compositing_mode_watcher_binding_.Bind(mojo::MakeRequest(&watcher_ptr));
    forwarding_mode_reporter_->AddCompositingModeWatcher(
        std::move(watcher_ptr));
  }
}

VizProcessTransportFactory::~VizProcessTransportFactory() {
  task_graph_runner_->Shutdown();
}

void VizProcessTransportFactory::ConnectHostFrameSinkManager() {
  viz::mojom::FrameSinkManagerPtr frame_sink_manager;
  viz::mojom::FrameSinkManagerRequest frame_sink_manager_request =
      mojo::MakeRequest(&frame_sink_manager);
  viz::mojom::FrameSinkManagerClientPtr frame_sink_manager_client;
  viz::mojom::FrameSinkManagerClientRequest frame_sink_manager_client_request =
      mojo::MakeRequest(&frame_sink_manager_client);

  // Setup HostFrameSinkManager with interface endpoints.
  GetHostFrameSinkManager()->BindAndSetManager(
      std::move(frame_sink_manager_client_request), resize_task_runner_,
      std::move(frame_sink_manager));

  // The ForwardingCompositingModeReporterImpl wants to watch the reporter in
  // the viz process. We give a mojo pointer to it over to that process and have
  // the viz process connect it to the reporter there directly, instead of
  // requesting a pointer to that reporter from this process.
  viz::mojom::CompositingModeWatcherPtr mode_watch_ptr =
      forwarding_mode_reporter_->BindAsWatcher();

  // Hop to the IO thread, then send the other side of interface to viz process.
  auto connect_on_io_thread =
      [](viz::mojom::FrameSinkManagerRequest request,
         viz::mojom::FrameSinkManagerClientPtrInfo client,
         viz::mojom::CompositingModeWatcherPtrInfo mode_watcher) {
        // TODO(kylechar): Check GpuProcessHost isn't null but don't enter a
        // restart loop.
        GpuProcessHost::Get()->ConnectFrameSinkManager(
            std::move(request), std::move(client), std::move(mode_watcher));
      };
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(connect_on_io_thread,
                     std::move(frame_sink_manager_request),
                     frame_sink_manager_client.PassInterface(),
                     mode_watch_ptr.PassInterface()));
}

void VizProcessTransportFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->UnregisterParent(
      compositor->widget());
#endif

  if (is_gpu_compositing_disabled_ || compositor->force_software_compositor()) {
    OnEstablishedGpuChannel(compositor, nullptr);
    return;
  }
  gpu_channel_establish_factory_->EstablishGpuChannel(
      base::BindOnce(&VizProcessTransportFactory::OnEstablishedGpuChannel,
                     weak_ptr_factory_.GetWeakPtr(), compositor));
}

scoped_refptr<viz::ContextProvider>
VizProcessTransportFactory::SharedMainThreadContextProvider() {
  NOTIMPLEMENTED();
  return nullptr;
}

void VizProcessTransportFactory::RemoveCompositor(ui::Compositor* compositor) {
#if defined(OS_WIN)
  // TODO(crbug.com/791660): Make sure that GpuProcessHost::SetChildSurface()
  // doesn't crash the GPU process after parent is unregistered.
  gfx::RenderingWindowManager::GetInstance()->UnregisterParent(
      compositor->widget());
#endif

  compositor_data_map_.erase(compositor);
}

double VizProcessTransportFactory::GetRefreshRate() const {
  // TODO(kylechar): Delete this function from ContextFactoryPrivate.
  return 60.0;
}

gpu::GpuMemoryBufferManager*
VizProcessTransportFactory::GetGpuMemoryBufferManager() {
  return gpu_channel_establish_factory_->GetGpuMemoryBufferManager();
}

cc::TaskGraphRunner* VizProcessTransportFactory::GetTaskGraphRunner() {
  return task_graph_runner_.get();
}

const viz::ResourceSettings& VizProcessTransportFactory::GetResourceSettings()
    const {
  return renderer_settings_.resource_settings;
}

void VizProcessTransportFactory::AddObserver(
    ui::ContextFactoryObserver* observer) {
  observer_list_.AddObserver(observer);
}

void VizProcessTransportFactory::RemoveObserver(
    ui::ContextFactoryObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

std::unique_ptr<ui::Reflector> VizProcessTransportFactory::CreateReflector(
    ui::Compositor* source,
    ui::Layer* target) {
  // TODO(crbug.com/601869): Reflector needs to be rewritten for viz.
  NOTIMPLEMENTED();
  return nullptr;
}

void VizProcessTransportFactory::RemoveReflector(ui::Reflector* reflector) {
  // TODO(crbug.com/601869): Reflector needs to be rewritten for viz.
  NOTIMPLEMENTED();
}

viz::FrameSinkId VizProcessTransportFactory::AllocateFrameSinkId() {
  return frame_sink_id_allocator_.NextFrameSinkId();
}

viz::HostFrameSinkManager*
VizProcessTransportFactory::GetHostFrameSinkManager() {
  return BrowserMainLoop::GetInstance()->host_frame_sink_manager();
}

void VizProcessTransportFactory::SetDisplayVisible(ui::Compositor* compositor,
                                                   bool visible) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end() || !iter->second.display_private)
    return;
  iter->second.display_private->SetDisplayVisible(visible);
}

void VizProcessTransportFactory::ResizeDisplay(ui::Compositor* compositor,
                                               const gfx::Size& size) {
  // Do nothing and resize when a CompositorFrame with a new size arrives.
}

void VizProcessTransportFactory::SetDisplayColorMatrix(
    ui::Compositor* compositor,
    const SkMatrix44& matrix) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end() || !iter->second.display_private)
    return;
  iter->second.display_private->SetDisplayColorMatrix(gfx::Transform(matrix));
}

void VizProcessTransportFactory::SetDisplayColorSpace(
    ui::Compositor* compositor,
    const gfx::ColorSpace& blending_color_space,
    const gfx::ColorSpace& output_color_space) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end() || !iter->second.display_private)
    return;
  iter->second.display_private->SetDisplayColorSpace(blending_color_space,
                                                     output_color_space);
}

void VizProcessTransportFactory::SetAuthoritativeVSyncInterval(
    ui::Compositor* compositor,
    base::TimeDelta interval) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end() || !iter->second.display_private)
    return;
  iter->second.display_private->SetAuthoritativeVSyncInterval(interval);
}

void VizProcessTransportFactory::SetDisplayVSyncParameters(
    ui::Compositor* compositor,
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  // TODO(crbug.com/772524): Deal with vsync later.
  NOTIMPLEMENTED();
}

void VizProcessTransportFactory::IssueExternalBeginFrame(
    ui::Compositor* compositor,
    const viz::BeginFrameArgs& args) {
  // TODO(crbug.com/772524): Deal with vsync later.
  NOTIMPLEMENTED();
}

void VizProcessTransportFactory::SetOutputIsSecure(ui::Compositor* compositor,
                                                   bool secure) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end() || !iter->second.display_private)
    return;
  iter->second.display_private->SetOutputIsSecure(secure);
}

viz::FrameSinkManagerImpl* VizProcessTransportFactory::GetFrameSinkManager() {
  // When running with viz there is no FrameSinkManagerImpl in the browser
  // process. FrameSinkManagerImpl runs in the GPU process instead. Anything in
  // the browser process that relies FrameSinkManagerImpl or SurfaceManager
  // internal state needs to change. See https://crbug.com/787097 and
  // https://crbug.com/760181 for more context.
  NOTREACHED();
  return nullptr;
}

bool VizProcessTransportFactory::IsGpuCompositingDisabled() {
  return is_gpu_compositing_disabled_;
}

ui::ContextFactory* VizProcessTransportFactory::GetContextFactory() {
  return this;
}

ui::ContextFactoryPrivate*
VizProcessTransportFactory::GetContextFactoryPrivate() {
  return this;
}

viz::GLHelper* VizProcessTransportFactory::GetGLHelper() {
  // TODO(kylechar): Figure out if GLHelper in the host process makes sense.
  NOTREACHED();
  return nullptr;
}

#if defined(OS_MACOSX)
void VizProcessTransportFactory::SetCompositorSuspendedForRecycle(
    ui::Compositor* compositor,
    bool suspended) {
  NOTIMPLEMENTED();
}
#endif

void VizProcessTransportFactory::CompositingModeFallbackToSoftware() {
  // This may happen multiple times, since when the viz process (re)starts, it
  // will send this notification if gpu is disabled.
  if (is_gpu_compositing_disabled_)
    return;

  // Change the result of IsGpuCompositingDisabled() before notifying anything.
  is_gpu_compositing_disabled_ = true;

  // Consumers of the shared main thread context aren't CompositingModeWatchers,
  // so inform them about the compositing mode switch by acting like the context
  // was lost. This also destroys the contexts since they aren't created when
  // gpu compositing isn't being used.
  OnLostMainThreadSharedContext();

  // Drop our reference on the gpu contexts for the compositors.
  shared_worker_context_provider_ = nullptr;
  compositor_context_provider_ = nullptr;

  // Here we remove the FrameSink from every compositor that needs to fall back
  // to software compositing.
  //
  // Releasing the FrameSink from the compositor will remove it from
  // |compositor_data_map_|, so we can't do that while iterating though the
  // collection.
  std::vector<ui::Compositor*> to_release;
  to_release.reserve(compositor_data_map_.size());
  for (auto& pair : compositor_data_map_) {
    ui::Compositor* compositor = pair.first;
    if (!compositor->force_software_compositor())
      to_release.push_back(compositor);
  }
  for (ui::Compositor* compositor : to_release) {
    // Compositor expects to be not visible when releasing its FrameSink.
    bool visible = compositor->IsVisible();
    compositor->SetVisible(false);
    gfx::AcceleratedWidget widget = compositor->ReleaseAcceleratedWidget();
    compositor->SetAcceleratedWidget(widget);
    if (visible)
      compositor->SetVisible(true);
  }
}

void VizProcessTransportFactory::OnGpuProcessLost() {
  // Reconnect HostFrameSinkManager to new GPU process.
  ConnectHostFrameSinkManager();

  OnLostMainThreadSharedContext();
}

void VizProcessTransportFactory::OnEstablishedGpuChannel(
    base::WeakPtr<ui::Compositor> compositor_weak_ptr,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  ui::Compositor* compositor = compositor_weak_ptr.get();
  if (!compositor)
    return;

  bool gpu_compositing =
      !is_gpu_compositing_disabled_ && !compositor->force_software_compositor();

  // Only try to make contexts for gpu compositing.
  if (gpu_compositing) {
    if (!gpu_channel_host ||
        !CreateContextProviders(std::move(gpu_channel_host))) {
      // Retry on failure. If this isn't possible we should hear that we're
      // falling back to software compositing from the viz process eventually.
      gpu_channel_establish_factory_->EstablishGpuChannel(
          base::BindOnce(&VizProcessTransportFactory::OnEstablishedGpuChannel,
                         weak_ptr_factory_.GetWeakPtr(), compositor_weak_ptr));
      return;
    }
  }

#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->RegisterParent(
      compositor->widget());
#endif

  // TODO(crbug.com/776050): Deal with context loss.

  // Create interfaces for a root CompositorFrameSink.
  viz::mojom::CompositorFrameSinkAssociatedPtrInfo sink_info;
  viz::mojom::CompositorFrameSinkAssociatedRequest sink_request =
      mojo::MakeRequest(&sink_info);
  viz::mojom::CompositorFrameSinkClientPtr client;
  viz::mojom::CompositorFrameSinkClientRequest client_request =
      mojo::MakeRequest(&client);
  viz::mojom::DisplayPrivateAssociatedRequest display_private_request =
      mojo::MakeRequest(&compositor_data_map_[compositor].display_private);
  compositor_data_map_[compositor].display_client =
      std::make_unique<InProcessDisplayClient>(compositor->widget());
  viz::mojom::DisplayClientPtr display_client =
      compositor_data_map_[compositor].display_client->GetBoundPtr();

#if defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
  gpu::SurfaceHandle surface_handle = compositor->widget();
#else
  // TODO(kylechar): Fix this when we support macOS.
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
#endif

  // Creates the viz end of the root CompositorFrameSink.
  GetHostFrameSinkManager()->CreateRootCompositorFrameSink(
      compositor->frame_sink_id(), surface_handle,
      compositor->force_software_compositor(), renderer_settings_,
      std::move(sink_request), std::move(client),
      std::move(display_private_request), std::move(display_client));

  // Create LayerTreeFrameSink with the browser end of CompositorFrameSink.
  viz::ClientLayerTreeFrameSink::InitParams params;
  params.compositor_task_runner = compositor->task_runner();
  params.gpu_memory_buffer_manager = GetGpuMemoryBufferManager();
  // TODO(crbug.com/730660): Make a ClientSharedBitmapManager to pass here.
  params.shared_bitmap_manager = shared_bitmap_manager_.get();
  params.pipes.compositor_frame_sink_associated_info = std::move(sink_info);
  params.pipes.client_request = std::move(client_request);
  params.local_surface_id_provider =
      std::make_unique<viz::DefaultLocalSurfaceIdProvider>();
  params.enable_surface_synchronization =
      features::IsSurfaceSynchronizationEnabled();

  scoped_refptr<ui::ContextProviderCommandBuffer> compositor_context;
  scoped_refptr<ui::ContextProviderCommandBuffer> worker_context;
  if (gpu_compositing) {
    // Only pass the contexts to the compositor if it will use gpu compositing.
    compositor_context = compositor_context_provider_;
    worker_context = shared_worker_context_provider_;
  }
  compositor->SetLayerTreeFrameSink(
      std::make_unique<viz::ClientLayerTreeFrameSink>(
          std::move(compositor_context), std::move(worker_context), &params));

#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->DoSetParentOnChild(
      compositor->widget());
#endif
}

bool VizProcessTransportFactory::CreateContextProviders(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  constexpr bool kSharedWorkerContextSupportsLocking = true;
  constexpr bool kSharedWorkerContextSupportsGLES2 = true;
  constexpr bool kSharedWorkerContextSupportsRaster = true;
  constexpr bool kCompositorContextSupportsLocking = false;
  constexpr bool kCompositorContextSupportsGLES2 = true;
  constexpr bool kCompositorContextSupportsRaster = false;

  if (CheckContextLost(compositor_context_provider_.get())) {
    // Both will be lost because they are in the same share group.
    shared_worker_context_provider_ = nullptr;
    compositor_context_provider_ = nullptr;
  }

  if (!shared_worker_context_provider_) {
    shared_worker_context_provider_ = CreateContextProviderImpl(
        gpu_channel_host, GetGpuMemoryBufferManager(),
        kSharedWorkerContextSupportsLocking, kSharedWorkerContextSupportsGLES2,
        kSharedWorkerContextSupportsRaster, nullptr,
        ui::command_buffer_metrics::BROWSER_WORKER_CONTEXT);

    auto result = shared_worker_context_provider_->BindToCurrentThread();
    if (result != gpu::ContextResult::kSuccess) {
      shared_worker_context_provider_ = nullptr;
      return false;
    }
  }

  if (!compositor_context_provider_) {
    compositor_context_provider_ = CreateContextProviderImpl(
        std::move(gpu_channel_host), GetGpuMemoryBufferManager(),
        kCompositorContextSupportsLocking, kCompositorContextSupportsGLES2,
        kCompositorContextSupportsRaster, shared_worker_context_provider_.get(),
        ui::command_buffer_metrics::UI_COMPOSITOR_CONTEXT);
    compositor_context_provider_->SetDefaultTaskRunner(resize_task_runner_);

    auto result = compositor_context_provider_->BindToCurrentThread();
    if (result != gpu::ContextResult::kSuccess) {
      compositor_context_provider_ = nullptr;
      shared_worker_context_provider_ = nullptr;
      return false;
    }
  }

  return true;
}

void VizProcessTransportFactory::OnLostMainThreadSharedContext() {
  // TODO(danakj): When we implement making the shared context, we'll also
  // have to recreate it here before calling OnLostResources().
  for (auto& observer : observer_list_)
    observer.OnLostResources();
}

VizProcessTransportFactory::CompositorData::CompositorData() = default;
VizProcessTransportFactory::CompositorData::CompositorData(
    CompositorData&& other) = default;
VizProcessTransportFactory::CompositorData::~CompositorData() = default;
VizProcessTransportFactory::CompositorData&
VizProcessTransportFactory::CompositorData::operator=(CompositorData&& other) =
    default;

}  // namespace content
