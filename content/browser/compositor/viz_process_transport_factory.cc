// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/viz_process_transport_factory.h"

#include <utility>

#include "base/single_thread_task_runner.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include "components/viz/client/client_layer_tree_frame_sink.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/host/renderer_settings_creation.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/common/gpu_stream_constants.h"
#include "content/public/common/content_client.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/compositor/reflector.h"

namespace content {
namespace {

// The client_id used here should not conflict with the client_id generated
// from RenderWidgetHostImpl.
constexpr uint32_t kDefaultClientId = 0u;

scoped_refptr<ui::ContextProviderCommandBuffer> CreateContextProviderImpl(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    bool support_locking,
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

  GURL url("chrome://gpu/VizProcessTransportFactory::CreateContextProvider");
  return base::MakeRefCounted<ui::ContextProviderCommandBuffer>(
      std::move(gpu_channel_host), kGpuStreamIdDefault, kGpuStreamPriorityUI,
      gpu::kNullSurfaceHandle, std::move(url), kAutomaticFlushes,
      support_locking, gpu::SharedMemoryLimits(), attributes,
      shared_context_provider, type);
}

}  // namespace

VizProcessTransportFactory::VizProcessTransportFactory(
    gpu::GpuChannelEstablishFactory* gpu_channel_establish_factory,
    scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner)
    : gpu_channel_establish_factory_(gpu_channel_establish_factory),
      resize_task_runner_(std::move(resize_task_runner)),
      frame_sink_id_allocator_(kDefaultClientId),
      task_graph_runner_(std::make_unique<cc::SingleThreadTaskGraphRunner>()),
      renderer_settings_(
          viz::CreateRendererSettings(CreateBufferToTextureTargetMap())),
      weak_ptr_factory_(this) {
  DCHECK(gpu_channel_establish_factory_);
  task_graph_runner_->Start("CompositorTileWorker1",
                            base::SimpleThread::Options());
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

  // Hop to the IO thread, then send the other side of interface to viz process.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(
                              [](viz::mojom::FrameSinkManagerRequest request,
                                 viz::mojom::FrameSinkManagerClientPtr client) {
                                GpuProcessHost::Get()->ConnectFrameSinkManager(
                                    std::move(request), std::move(client));
                              },
                              std::move(frame_sink_manager_request),
                              std::move(frame_sink_manager_client)));
}

void VizProcessTransportFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  gpu_channel_establish_factory_->EstablishGpuChannel(base::Bind(
      &VizProcessTransportFactory::CreateLayerTreeFrameSinkForGpuChannel,
      weak_ptr_factory_.GetWeakPtr(), compositor));
}

scoped_refptr<viz::ContextProvider>
VizProcessTransportFactory::SharedMainThreadContextProvider() {
  NOTIMPLEMENTED();
  return nullptr;
}

void VizProcessTransportFactory::RemoveCompositor(ui::Compositor* compositor) {
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
  // TODO(crbug.com/772524): Deal with vsync later.
  NOTIMPLEMENTED();
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
  // FrameSinkManagerImpl is in the gpu process, not the browser process.
  NOTREACHED();
  return nullptr;
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

void VizProcessTransportFactory::CreateLayerTreeFrameSinkForGpuChannel(
    base::WeakPtr<ui::Compositor> compositor_weak_ptr,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  ui::Compositor* compositor = compositor_weak_ptr.get();
  if (!compositor)
    return;

  scoped_refptr<ui::ContextProviderCommandBuffer> context_provider =
      CreateContextProvider(std::move(gpu_channel_host));

  if (!context_provider) {
    // TODO(kylechar): Retry ContextProvider creation if it failed.
    NOTIMPLEMENTED();
    return;
  }

  // TODO(crbug.com/776050): Deal with context loss and GPU restartability.

  // Create interfaces for a root CompositorFrameSink.
  viz::mojom::CompositorFrameSinkAssociatedPtrInfo sink_info;
  viz::mojom::CompositorFrameSinkAssociatedRequest sink_request =
      mojo::MakeRequest(&sink_info);
  viz::mojom::CompositorFrameSinkClientPtr client;
  viz::mojom::CompositorFrameSinkClientRequest client_request =
      mojo::MakeRequest(&client);
  viz::mojom::DisplayPrivateAssociatedRequest display_private_request =
      mojo::MakeRequest(&compositor_data_map_[compositor].display_private);

#if defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
  gpu::SurfaceHandle surface_handle = compositor->widget();
#else
  // TODO(kylechar): Fix this when we support macOS.
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
#endif

  // Creates the viz end of the root CompositorFrameSink.
  GetHostFrameSinkManager()->CreateRootCompositorFrameSink(
      compositor->frame_sink_id(), surface_handle, renderer_settings_,
      std::move(sink_request), std::move(client),
      std::move(display_private_request));

  // Create LayerTreeFrameSink with the browser end of CompositorFrameSink.
  viz::ClientLayerTreeFrameSink::InitParams params;
  params.gpu_memory_buffer_manager = GetGpuMemoryBufferManager();
  params.pipes.compositor_frame_sink_associated_info = std::move(sink_info);
  params.pipes.client_request = std::move(client_request);
  params.local_surface_id_provider =
      std::make_unique<viz::DefaultLocalSurfaceIdProvider>();

  compositor->SetLayerTreeFrameSink(
      std::make_unique<viz::ClientLayerTreeFrameSink>(
          std::move(context_provider), shared_worker_context_provider_,
          &params));
}

scoped_refptr<ui::ContextProviderCommandBuffer>
VizProcessTransportFactory::CreateContextProvider(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  // TODO(kylechar): Check if |shared_worker_context_provider_| is lost.
  if (!shared_worker_context_provider_) {
    shared_worker_context_provider_ = CreateContextProviderImpl(
        gpu_channel_host, true /* support_locking */, nullptr,
        ui::command_buffer_metrics::BROWSER_WORKER_CONTEXT);

    auto result = shared_worker_context_provider_->BindToCurrentThread();
    if (result != gpu::ContextResult::kSuccess) {
      shared_worker_context_provider_ = nullptr;
      return nullptr;
    }
  }

  // TODO(crbug.com/776052): Use the same ContextProvider for all compositors.
  scoped_refptr<ui::ContextProviderCommandBuffer> context_provider =
      CreateContextProviderImpl(
          std::move(gpu_channel_host), false /* support_locking */,
          shared_worker_context_provider_.get(),
          ui::command_buffer_metrics::UI_COMPOSITOR_CONTEXT);
  context_provider->SetDefaultTaskRunner(resize_task_runner_);

  auto result = context_provider->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess)
    return nullptr;

  return context_provider;
}

VizProcessTransportFactory::CompositorData::CompositorData() = default;
VizProcessTransportFactory::CompositorData::CompositorData(
    CompositorData&& other) = default;
VizProcessTransportFactory::CompositorData::~CompositorData() = default;
VizProcessTransportFactory::CompositorData&
VizProcessTransportFactory::CompositorData::operator=(CompositorData&& other) =
    default;

}  // namespace content
