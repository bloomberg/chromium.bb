// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MUS_RENDERER_WINDOW_TREE_CLIENT_H_
#define CONTENT_RENDERER_MUS_RENDERER_WINDOW_TREE_CLIENT_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/common/types.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"

namespace cc {
class LayerTreeFrameSink;
class ContextProvider;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace content {

// ui.mojom.WindowTreeClient implementation for RenderWidget. This lives and
// operates on the renderer's main thread.
class RendererWindowTreeClient : public ui::mojom::WindowTreeClient {
 public:
  // Creates a RendererWindowTreeClient instance for the RenderWidget instance
  // associated with |routing_id| (if one doesn't already exist). The instance
  // self-destructs when the connection to mus is lost, or when the window is
  // closed.
  static void CreateIfNecessary(int routing_id);

  // Destroys the client instance, if one exists. Otherwise, does nothing.
  static void Destroy(int routing_id);

  // Returns the RendererWindowTreeClient associated with |routing_id|. Returns
  // nullptr if none exists.
  static RendererWindowTreeClient* Get(int routing_id);

  void Bind(ui::mojom::WindowTreeClientRequest request);

  using LayerTreeFrameSinkCallback =
      base::Callback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>;
  void RequestLayerTreeFrameSink(
      scoped_refptr<cc::ContextProvider> context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const LayerTreeFrameSinkCallback& callback);

 private:
  explicit RendererWindowTreeClient(int routing_id);
  ~RendererWindowTreeClient() override;

  void RequestLayerTreeFrameSinkInternal(
      scoped_refptr<cc::ContextProvider> context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const LayerTreeFrameSinkCallback& callback);

  void DestroySelf();

  // ui::mojom::WindowTreeClient:
  // Note: A number of the following are currently not-implemented. Some of
  // these will remain unimplemented in the long-term. Some of the
  // implementations would require some amount of refactoring out of
  // RenderWidget and related classes (e.g. resize, input, ime etc.).
  void OnEmbed(
      ui::ClientSpecificId client_id,
      ui::mojom::WindowDataPtr root,
      ui::mojom::WindowTreePtr tree,
      int64_t display_id,
      ui::Id focused_window_id,
      bool drawn,
      const base::Optional<cc::LocalSurfaceId>& local_surface_id) override;
  void OnEmbeddedAppDisconnected(ui::Id window_id) override;
  void OnUnembed(ui::Id window_id) override;
  void OnCaptureChanged(ui::Id new_capture_window_id,
                        ui::Id old_capture_window_id) override;
  void OnFrameSinkIdAllocated(ui::Id window_id,
                              const cc::FrameSinkId& frame_sink_id) override;
  void OnTopLevelCreated(
      uint32_t change_id,
      ui::mojom::WindowDataPtr data,
      int64_t display_id,
      bool drawn,
      const base::Optional<cc::LocalSurfaceId>& local_surface_id) override;
  void OnWindowBoundsChanged(
      ui::Id window_id,
      const gfx::Rect& old_bounds,
      const gfx::Rect& new_bounds,
      const base::Optional<cc::LocalSurfaceId>& local_frame_id) override;
  void OnWindowTransformChanged(ui::Id window_id,
                                const gfx::Transform& old_transform,
                                const gfx::Transform& new_transform) override;
  void OnClientAreaChanged(
      uint32_t window_id,
      const gfx::Insets& new_client_area,
      const std::vector<gfx::Rect>& new_additional_client_areas) override;
  void OnTransientWindowAdded(uint32_t window_id,
                              uint32_t transient_window_id) override;
  void OnTransientWindowRemoved(uint32_t window_id,
                                uint32_t transient_window_id) override;
  void OnWindowHierarchyChanged(
      ui::Id window_id,
      ui::Id old_parent_id,
      ui::Id new_parent_id,
      std::vector<ui::mojom::WindowDataPtr> windows) override;
  void OnWindowReordered(ui::Id window_id,
                         ui::Id relative_window_id,
                         ui::mojom::OrderDirection direction) override;
  void OnWindowDeleted(ui::Id window_id) override;
  void OnWindowVisibilityChanged(ui::Id window_id, bool visible) override;
  void OnWindowOpacityChanged(ui::Id window_id,
                              float old_opacity,
                              float new_opacity) override;
  void OnWindowParentDrawnStateChanged(ui::Id window_id, bool drawn) override;
  void OnWindowSharedPropertyChanged(
      ui::Id window_id,
      const std::string& name,
      const base::Optional<std::vector<uint8_t>>& new_data) override;
  void OnWindowInputEvent(uint32_t event_id,
                          ui::Id window_id,
                          int64_t display_id,
                          std::unique_ptr<ui::Event> event,
                          bool matches_pointer_watcher) override;
  void OnPointerEventObserved(std::unique_ptr<ui::Event> event,
                              uint32_t window_id,
                              int64_t display_id) override;
  void OnWindowFocused(ui::Id focused_window_id) override;
  void OnWindowCursorChanged(ui::Id window_id, ui::CursorData cursor) override;
  void OnWindowSurfaceChanged(ui::Id window_id,
                              const cc::SurfaceInfo& surface_info) override;
  void OnDragDropStart(
      const std::unordered_map<std::string, std::vector<uint8_t>>& mime_data)
      override;
  void OnDragEnter(ui::Id window_id,
                   uint32_t event_flags,
                   const gfx::Point& position,
                   uint32_t effect_bitmask,
                   const OnDragEnterCallback& callback) override;
  void OnDragOver(ui::Id window_id,
                  uint32_t event_flags,
                  const gfx::Point& position,
                  uint32_t effect_bitmask,
                  const OnDragOverCallback& callback) override;
  void OnDragLeave(ui::Id window_id) override;
  void OnCompleteDrop(ui::Id window_id,
                      uint32_t event_flags,
                      const gfx::Point& position,
                      uint32_t effect_bitmask,
                      const OnCompleteDropCallback& callback) override;
  void OnPerformDragDropCompleted(uint32_t window,
                                  bool success,
                                  uint32_t action_taken) override;
  void OnDragDropDone() override;
  void OnChangeCompleted(uint32_t change_id, bool success) override;
  void RequestClose(uint32_t window_id) override;
  void GetWindowManager(
      mojo::AssociatedInterfaceRequest<ui::mojom::WindowManager> internal)
      override;

  const int routing_id_;
  ui::Id root_window_id_;
  scoped_refptr<cc::ContextProvider> pending_context_provider_;
  gpu::GpuMemoryBufferManager* pending_gpu_memory_buffer_manager_ = nullptr;
  LayerTreeFrameSinkCallback pending_layer_tree_frame_sink_callback_;
  ui::mojom::WindowTreePtr tree_;
  mojo::Binding<ui::mojom::WindowTreeClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(RendererWindowTreeClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MUS_RENDERER_WINDOW_TREE_CLIENT_H_
