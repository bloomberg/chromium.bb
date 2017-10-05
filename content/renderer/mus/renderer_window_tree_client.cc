// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mus/renderer_window_tree_client.h"

#include <map>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "cc/base/switches.h"
#include "components/viz/client/client_layer_tree_frame_sink.h"
#include "components/viz/client/hit_test_data_provider.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "content/renderer/mash_util.h"
#include "content/renderer/render_frame_proxy.h"

namespace content {

namespace {

using ConnectionMap = std::map<int, RendererWindowTreeClient*>;
base::LazyInstance<ConnectionMap>::Leaky g_connections =
    LAZY_INSTANCE_INITIALIZER;

// Callback from embedding a child frame.
void OnEmbedAck(bool success) {
  DCHECK(success);
}

}  // namespace

// static
void RendererWindowTreeClient::CreateIfNecessary(int routing_id) {
  if (!IsRunningInMash() || Get(routing_id))
    return;
  RendererWindowTreeClient* connection =
      new RendererWindowTreeClient(routing_id);
  g_connections.Get().insert(std::make_pair(routing_id, connection));
}

// static
void RendererWindowTreeClient::Destroy(int routing_id) {
  delete Get(routing_id);
}

// static
RendererWindowTreeClient* RendererWindowTreeClient::Get(int routing_id) {
  auto it = g_connections.Get().find(routing_id);
  if (it != g_connections.Get().end())
    return it->second;
  return nullptr;
}

void RendererWindowTreeClient::Bind(
    ui::mojom::WindowTreeClientRequest request,
    mojom::RenderWidgetWindowTreeClientRequest
        render_widget_window_tree_client_request) {
  binding_.Bind(std::move(request));
  render_widget_window_tree_client_binding_.Bind(
      std::move(render_widget_window_tree_client_request));
}

std::unique_ptr<MusEmbeddedFrame>
RendererWindowTreeClient::OnRenderFrameProxyCreated(
    RenderFrameProxy* render_frame_proxy) {
  auto iter = pending_frames_.find(render_frame_proxy->routing_id());
  if (iter == pending_frames_.end())
    return nullptr;

  const base::UnguessableToken token = iter->second;
  pending_frames_.erase(iter);
  return CreateMusEmbeddedFrame(render_frame_proxy, token);
}

void RendererWindowTreeClient::SetVisible(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  // TODO(sky): determine when window should be updated.
}

void RendererWindowTreeClient::RequestLayerTreeFrameSink(
    scoped_refptr<viz::ContextProvider> context_provider,
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
    : routing_id_(routing_id),
      binding_(this),
      render_widget_window_tree_client_binding_(this) {}

RendererWindowTreeClient::~RendererWindowTreeClient() {
  g_connections.Get().erase(routing_id_);
  DCHECK(embedded_frames_.empty());
}

std::unique_ptr<MusEmbeddedFrame>
RendererWindowTreeClient::CreateMusEmbeddedFrame(
    RenderFrameProxy* render_frame_proxy,
    const base::UnguessableToken& token) {
  std::unique_ptr<MusEmbeddedFrame> frame = base::WrapUnique<MusEmbeddedFrame>(
      new MusEmbeddedFrame(this, render_frame_proxy, ++next_window_id_, token));
  embedded_frames_.insert(frame.get());
  return frame;
}

void RendererWindowTreeClient::RequestLayerTreeFrameSinkInternal(
    scoped_refptr<viz::ContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const LayerTreeFrameSinkCallback& callback) {
  viz::mojom::CompositorFrameSinkPtrInfo sink_info;
  viz::mojom::CompositorFrameSinkRequest sink_request =
      mojo::MakeRequest(&sink_info);
  viz::mojom::CompositorFrameSinkClientPtr client;
  viz::mojom::CompositorFrameSinkClientRequest client_request =
      mojo::MakeRequest(&client);
  viz::ClientLayerTreeFrameSink::InitParams params;
  params.gpu_memory_buffer_manager = gpu_memory_buffer_manager;
  params.pipes.compositor_frame_sink_info = std::move(sink_info);
  params.pipes.client_request = std::move(client_request);
  params.local_surface_id_provider =
      base::MakeUnique<viz::DefaultLocalSurfaceIdProvider>();
  params.enable_surface_synchronization = true;
  auto frame_sink = base::MakeUnique<viz::ClientLayerTreeFrameSink>(
      std::move(context_provider), nullptr /* worker_context_provider */,
      &params);
  tree_->AttachCompositorFrameSink(root_window_id_, std::move(sink_request),
                                   std::move(client));
  callback.Run(std::move(frame_sink));
}

void RendererWindowTreeClient::OnEmbeddedFrameDestroyed(
    MusEmbeddedFrame* frame) {
  embedded_frames_.erase(embedded_frames_.find(frame));
}

void RendererWindowTreeClient::Embed(uint32_t frame_routing_id,
                                     const base::UnguessableToken& token) {
  RenderFrameProxy* render_frame_proxy =
      RenderFrameProxy::FromRoutingID(frame_routing_id);
  if (!render_frame_proxy) {
    pending_frames_[frame_routing_id] = token;
    return;
  }
  render_frame_proxy->SetMusEmbeddedFrame(
      CreateMusEmbeddedFrame(render_frame_proxy, token));
}

void RendererWindowTreeClient::DestroyFrame(uint32_t frame_routing_id) {
  pending_frames_.erase(frame_routing_id);
}

void RendererWindowTreeClient::OnEmbed(
    ui::mojom::WindowDataPtr root,
    ui::mojom::WindowTreePtr tree,
    int64_t display_id,
    ui::Id focused_window_id,
    bool drawn,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  root_window_id_ = root->window_id;
  tree_ = std::move(tree);
  // TODO(sky): determine when window should be updated.
  for (MusEmbeddedFrame* frame : embedded_frames_)
    frame->OnTreeAvailable();

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
  // At this point all operations will fail. We don't delete this as it would
  // mean all consumers have to null check (as would MusEmbeddedFrames).
}

void RendererWindowTreeClient::OnCaptureChanged(ui::Id new_capture_window_id,
                                                ui::Id old_capture_window_id) {}

void RendererWindowTreeClient::OnFrameSinkIdAllocated(
    ui::Id window_id,
    const viz::FrameSinkId& frame_sink_id) {
  for (MusEmbeddedFrame* embedded_frame : embedded_frames_) {
    if (embedded_frame->window_id_ == window_id) {
      embedded_frame->render_frame_proxy_->OnMusFrameSinkIdAllocated(
          frame_sink_id);
      return;
    }
  }
}

void RendererWindowTreeClient::OnTopLevelCreated(
    uint32_t change_id,
    ui::mojom::WindowDataPtr data,
    int64_t display_id,
    bool drawn,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  NOTREACHED();
}

void RendererWindowTreeClient::OnWindowBoundsChanged(
    ui::Id window_id,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {}

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
  // See comments on OnUnembed() for why we do nothing here.
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
    const gfx::PointF& event_location_in_screen_pixel_layout,
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
    const viz::SurfaceInfo& surface_info) {
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
                                                 bool success) {
  // Don't DCHECK success, as it's possible we'll try to do some operations
  // after unembedded, which means all operations will fail.
}

void RendererWindowTreeClient::RequestClose(uint32_t window_id) {}

void RendererWindowTreeClient::GetWindowManager(
    mojo::AssociatedInterfaceRequest<ui::mojom::WindowManager> internal) {
  NOTREACHED();
}

// MusEmbeddedFrame ------------------------------------------------------------

MusEmbeddedFrame::~MusEmbeddedFrame() {
  renderer_window_tree_client_->OnEmbeddedFrameDestroyed(this);
  // If there is |pending_state_| it means we didn't actually create the window
  // yet and there is nothing to do.
  if (pending_state_)
    return;

  window_tree()->DeleteWindow(GetAndAdvanceNextChangeId(), window_id_);
}

void MusEmbeddedFrame::SetWindowBounds(
    const viz::LocalSurfaceId& local_surface_id,
    const gfx::Rect& bounds) {
  if (!window_tree()) {
    DCHECK(pending_state_);
    pending_state_->bounds = bounds;
    pending_state_->local_surface_id = local_surface_id;
    pending_state_->was_set_window_bounds_called = true;
    return;
  }

  window_tree()->SetWindowBounds(GetAndAdvanceNextChangeId(), window_id_,
                                 bounds, local_surface_id);
}

MusEmbeddedFrame::MusEmbeddedFrame(
    RendererWindowTreeClient* renderer_window_tree_client,
    RenderFrameProxy* proxy,
    ui::ClientSpecificId window_id,
    const base::UnguessableToken& token)
    : renderer_window_tree_client_(renderer_window_tree_client),
      render_frame_proxy_(proxy),
      window_id_(window_id) {
  if (!window_tree()) {
    pending_state_ = base::MakeUnique<PendingState>();
    pending_state_->token = token;
    return;
  }
  CreateChildWindowAndEmbed(token);
}

void MusEmbeddedFrame::CreateChildWindowAndEmbed(
    const base::UnguessableToken& token) {
  window_tree()->NewWindow(GetAndAdvanceNextChangeId(), window_id_,
                           base::nullopt);
  window_tree()->AddWindow(GetAndAdvanceNextChangeId(),
                           renderer_window_tree_client_->root_window_id_,
                           window_id_);
  window_tree()->EmbedUsingToken(window_id_, token, 0, base::Bind(&OnEmbedAck));
}

void MusEmbeddedFrame::OnTreeAvailable() {
  std::unique_ptr<PendingState> pending_state = std::move(pending_state_);
  CreateChildWindowAndEmbed(pending_state->token);
  if (pending_state->was_set_window_bounds_called)
    SetWindowBounds(pending_state->local_surface_id, pending_state->bounds);
}

uint32_t MusEmbeddedFrame::GetAndAdvanceNextChangeId() {
  return renderer_window_tree_client_->GetAndAdvanceNextChangeId();
}

MusEmbeddedFrame::PendingState::PendingState() = default;

MusEmbeddedFrame::PendingState::~PendingState() = default;

}  // namespace content
