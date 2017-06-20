// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mus/renderer_window_tree_client.h"

#include <map>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "cc/base/switches.h"
#include "components/viz/client/client_layer_tree_frame_sink.h"
#include "components/viz/client/local_surface_id_provider.h"

namespace content {

namespace {
typedef std::map<int, RendererWindowTreeClient*> ConnectionMap;
base::LazyInstance<ConnectionMap>::Leaky g_connections =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
RendererWindowTreeClient* RendererWindowTreeClient::Get(int routing_id) {
  auto it = g_connections.Get().find(routing_id);
  if (it != g_connections.Get().end())
    return it->second;
  return nullptr;
}

// static
void RendererWindowTreeClient::CreateIfNecessary(int routing_id) {
  if (Get(routing_id))
    return;
  RendererWindowTreeClient* connection =
      new RendererWindowTreeClient(routing_id);
  g_connections.Get().insert(std::make_pair(routing_id, connection));
}

// static
void RendererWindowTreeClient::Destroy(int routing_id) {
  auto* client = Get(routing_id);
  if (client)
    client->DestroySelf();
}

void RendererWindowTreeClient::Bind(
    ui::mojom::WindowTreeClientRequest request) {
  binding_.Bind(std::move(request));
}

void RendererWindowTreeClient::RequestLayerTreeFrameSink(
    scoped_refptr<cc::ContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const LayerTreeFrameSinkCallback& callback) {
  DCHECK(pending_layer_tree_frame_sink_callback_.is_null());
  if (tree_) {
    RequestLayerTreeFrameSinkInternal(std::move(context_provider),
                                      gpu_memory_buffer_manager, callback);
    return;
  }

  pending_context_provider_ = std::move(context_provider);
  pending_gpu_memory_buffer_manager_ = gpu_memory_buffer_manager;
  pending_layer_tree_frame_sink_callback_ = callback;
}

RendererWindowTreeClient::RendererWindowTreeClient(int routing_id)
    : routing_id_(routing_id), binding_(this) {
}

RendererWindowTreeClient::~RendererWindowTreeClient() {
  g_connections.Get().erase(routing_id_);
}

void RendererWindowTreeClient::RequestLayerTreeFrameSinkInternal(
    scoped_refptr<cc::ContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const LayerTreeFrameSinkCallback& callback) {
  cc::mojom::MojoCompositorFrameSinkPtrInfo sink_info;
  cc::mojom::MojoCompositorFrameSinkRequest sink_request =
      mojo::MakeRequest(&sink_info);
  cc::mojom::MojoCompositorFrameSinkClientPtr client;
  cc::mojom::MojoCompositorFrameSinkClientRequest client_request =
      mojo::MakeRequest(&client);
  constexpr bool enable_surface_synchronization = true;
  auto frame_sink = base::MakeUnique<viz::ClientLayerTreeFrameSink>(
      std::move(context_provider), nullptr /* worker_context_provider */,
      gpu_memory_buffer_manager, nullptr /* shared_bitmap_manager */,
      nullptr /* synthetic_begin_frame_source */, std::move(sink_info),
      std::move(client_request),
      base::MakeUnique<viz::DefaultLocalSurfaceIdProvider>(),
      enable_surface_synchronization);
  tree_->AttachCompositorFrameSink(root_window_id_, std::move(sink_request),
                                   std::move(client));
  callback.Run(std::move(frame_sink));
}

void RendererWindowTreeClient::DestroySelf() {
  delete this;
}

void RendererWindowTreeClient::OnEmbed(
    ui::ClientSpecificId client_id,
    ui::mojom::WindowDataPtr root,
    ui::mojom::WindowTreePtr tree,
    int64_t display_id,
    ui::Id focused_window_id,
    bool drawn,
    const base::Optional<cc::LocalSurfaceId>& local_surface_id) {
  root_window_id_ = root->window_id;
  tree_ = std::move(tree);
  if (!pending_layer_tree_frame_sink_callback_.is_null()) {
    RequestLayerTreeFrameSinkInternal(std::move(pending_context_provider_),
                                      pending_gpu_memory_buffer_manager_,
                                      pending_layer_tree_frame_sink_callback_);
    pending_context_provider_ = nullptr;
    pending_gpu_memory_buffer_manager_ = nullptr;
    pending_layer_tree_frame_sink_callback_.Reset();
  }
}

void RendererWindowTreeClient::OnEmbeddedAppDisconnected(ui::Id window_id) {
  // TODO(sad): Embedded mus-client (oopif) is gone. Figure out what to do.
}

void RendererWindowTreeClient::OnUnembed(ui::Id window_id) {
  CHECK_EQ(window_id, root_window_id_);
  DestroySelf();
}

void RendererWindowTreeClient::OnCaptureChanged(ui::Id new_capture_window_id,
                                                ui::Id old_capture_window_id) {}

void RendererWindowTreeClient::OnFrameSinkIdAllocated(
    ui::Id window_id,
    const cc::FrameSinkId& frame_sink_id) {
  // TODO(fsamuel): OOPIF's |frame_sink_id| is ready. The OOPIF can now be
  // embedded by the parent.
}

void RendererWindowTreeClient::OnTopLevelCreated(
    uint32_t change_id,
    ui::mojom::WindowDataPtr data,
    int64_t display_id,
    bool drawn,
    const base::Optional<cc::LocalSurfaceId>& local_surface_id) {
  NOTREACHED();
}

void RendererWindowTreeClient::OnWindowBoundsChanged(
    ui::Id window_id,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    const base::Optional<cc::LocalSurfaceId>& local_surface_id) {
}

void RendererWindowTreeClient::OnWindowTransformChanged(
    ui::Id window_id,
    const gfx::Transform& old_transform,
    const gfx::Transform& new_transform) {}

void RendererWindowTreeClient::OnClientAreaChanged(
    uint32_t window_id,
    const gfx::Insets& new_client_area,
    const std::vector<gfx::Rect>& new_additional_client_areas) {}

void RendererWindowTreeClient::OnTransientWindowAdded(
    uint32_t window_id,
    uint32_t transient_window_id) {}

void RendererWindowTreeClient::OnTransientWindowRemoved(
    uint32_t window_id,
    uint32_t transient_window_id) {}

void RendererWindowTreeClient::OnWindowHierarchyChanged(
    ui::Id window_id,
    ui::Id old_parent_id,
    ui::Id new_parent_id,
    std::vector<ui::mojom::WindowDataPtr> windows) {}

void RendererWindowTreeClient::OnWindowReordered(
    ui::Id window_id,
    ui::Id relative_window_id,
    ui::mojom::OrderDirection direction) {}

void RendererWindowTreeClient::OnWindowDeleted(ui::Id window_id) {
  // TODO(sad): With OOPIF, |window_id| may not be |root_window_id_|. We need to
  // make sure that works correctly.
  CHECK_EQ(window_id, root_window_id_);
  DestroySelf();
}

void RendererWindowTreeClient::OnWindowVisibilityChanged(ui::Id window_id,
                                                         bool visible) {}

void RendererWindowTreeClient::OnWindowOpacityChanged(ui::Id window_id,
                                                      float old_opacity,
                                                      float new_opacity) {}

void RendererWindowTreeClient::OnWindowParentDrawnStateChanged(ui::Id window_id,
                                                               bool drawn) {}

void RendererWindowTreeClient::OnWindowSharedPropertyChanged(
    ui::Id window_id,
    const std::string& name,
    const base::Optional<std::vector<uint8_t>>& new_data) {}

void RendererWindowTreeClient::OnWindowInputEvent(
    uint32_t event_id,
    ui::Id window_id,
    int64_t display_id,
    std::unique_ptr<ui::Event> event,
    bool matches_pointer_watcher) {
  NOTREACHED();
}

void RendererWindowTreeClient::OnPointerEventObserved(
    std::unique_ptr<ui::Event> event,
    uint32_t window_id,
    int64_t display_id) {
  NOTREACHED();
}

void RendererWindowTreeClient::OnWindowFocused(ui::Id focused_window_id) {}

void RendererWindowTreeClient::OnWindowCursorChanged(ui::Id window_id,
                                                     ui::CursorData cursor) {}

void RendererWindowTreeClient::OnWindowSurfaceChanged(
    ui::Id window_id,
    const cc::SurfaceInfo& surface_info) {
  NOTIMPLEMENTED();
}

void RendererWindowTreeClient::OnDragDropStart(
    const std::unordered_map<std::string, std::vector<uint8_t>>& mime_data) {}

void RendererWindowTreeClient::OnDragEnter(
    ui::Id window_id,
    uint32_t event_flags,
    const gfx::Point& position,
    uint32_t effect_bitmask,
    const OnDragEnterCallback& callback) {}

void RendererWindowTreeClient::OnDragOver(ui::Id window_id,
                                          uint32_t event_flags,
                                          const gfx::Point& position,
                                          uint32_t effect_bitmask,
                                          const OnDragOverCallback& callback) {}

void RendererWindowTreeClient::OnDragLeave(ui::Id window_id) {}

void RendererWindowTreeClient::OnCompleteDrop(
    ui::Id window_id,
    uint32_t event_flags,
    const gfx::Point& position,
    uint32_t effect_bitmask,
    const OnCompleteDropCallback& callback) {}

void RendererWindowTreeClient::OnPerformDragDropCompleted(
    uint32_t window,
    bool success,
    uint32_t action_taken) {}

void RendererWindowTreeClient::OnDragDropDone() {}

void RendererWindowTreeClient::OnChangeCompleted(uint32_t change_id,
                                                 bool success) {}

void RendererWindowTreeClient::RequestClose(uint32_t window_id) {}

void RendererWindowTreeClient::GetWindowManager(
    mojo::AssociatedInterfaceRequest<ui::mojom::WindowManager> internal) {
  NOTREACHED();
}

}  // namespace content
