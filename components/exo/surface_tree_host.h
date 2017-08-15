// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_TREE_HOST_H_
#define COMPONENTS_EXO_SURFACE_TREE_HOST_H_

#include <memory>

#include "base/macros.h"
#include "components/exo/layer_tree_frame_sink_holder.h"
#include "components/exo/surface.h"
#include "components/exo/surface_delegate.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
class WindowDelegate;
}  // namespace aura

namespace gfx {
class Path;
}  // namespace gfx

namespace viz {
class BeginFrameSource;
}  // namespace viz

namespace exo {
class LayerTreeFrameSinkHolder;

// This class provides functionality for hosting a surface tree. The surface
// tree is hosted in the |host_window_|.
class SurfaceTreeHost : public SurfaceDelegate,
                        public aura::WindowObserver,
                        public viz::BeginFrameObserverBase,
                        public ui::CompositorVSyncManager::Observer,
                        public ui::ContextFactoryObserver {
 public:
  SurfaceTreeHost(const std::string& window_name,
                  aura::WindowDelegate* window_delegate);
  ~SurfaceTreeHost() override;

  // Sets a root surface of a surface tree. This surface tree will be hosted in
  // the |host_window_|.
  void SetRootSurface(Surface* root_surface);

  // Returns true if hosted surface tree has hittest mask.
  bool HasHitTestMask() const;

  // Returns the hittest mask in |mask| for the hosted surface tree.
  void GetHitTestMask(gfx::Path* mask) const;

  // Returns the bounds of the current input region of the hosted surface tree.
  gfx::Rect GetHitTestBounds() const;

  // Returns the cursor for the given position. If no cursor provider is
  // registered then CursorType::kNull is returned.
  gfx::NativeCursor GetCursor(const gfx::Point& point) const;

  // Call this to indicate that the previous CompositorFrame is processed and
  // the surface is being scheduled for a draw.
  void DidReceiveCompositorFrameAck();

  // Called when the begin frame source has changed.
  void SetBeginFrameSource(viz::BeginFrameSource* begin_frame_source);

  // Adds/Removes begin frame observer based on state.
  void UpdateNeedsBeginFrame();

  aura::Window* host_window() { return host_window_.get(); }
  const aura::Window* host_window() const { return host_window_.get(); }

  Surface* root_surface() { return root_surface_; }
  const Surface* root_surface() const { return root_surface_; }

  LayerTreeFrameSinkHolder* layer_tree_frame_sink_holder() {
    return layer_tree_frame_sink_holder_.get();
  }

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;
  void OnSurfaceContentSizeChanged() override;
  bool IsSurfaceSynchronized() const override;

  // Overridden from aura::WindowObserver:
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Overridden from cc::BeginFrameObserverBase:
  bool OnBeginFrameDerivedImpl(const viz::BeginFrameArgs& args) override;
  void OnBeginFrameSourcePausedChanged(bool paused) override {}

  // Overridden from ui::CompositorVSyncManager::Observer:
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override;

  // Overridden from ui::ContextFactoryObserver:
  void OnLostResources() override;

 private:
  void SubmitCompositorFrame(Surface::FrameType frame_type);

  Surface* root_surface_ = nullptr;
  std::unique_ptr<aura::Window> host_window_;
  std::unique_ptr<LayerTreeFrameSinkHolder> layer_tree_frame_sink_holder_;

  // The begin frame source being observed.
  viz::BeginFrameSource* begin_frame_source_ = nullptr;
  bool needs_begin_frame_ = false;
  viz::BeginFrameAck current_begin_frame_ack_;

  // These lists contain the callbacks to notify the client when it is a good
  // time to start producing a new frame. These callbacks move to
  // |frame_callbacks_| when Commit() is called. Later they are moved to
  // |active_frame_callbacks_| when the effect of the Commit() is scheduled to
  // be drawn. They fire at the first begin frame notification after this.
  std::list<Surface::FrameCallback> frame_callbacks_;
  std::list<Surface::FrameCallback> active_frame_callbacks_;

  // These lists contains the callbacks to notify the client when surface
  // contents have been presented. These callbacks move to
  // |presentation_callbacks_| when Commit() is called. Later they are moved to
  // |swapping_presentation_callbacks_| when the effect of the Commit() is
  // scheduled to be drawn and then moved to |swapped_presentation_callbacks_|
  // after receiving VSync parameters update for the previous frame. They fire
  // at the next VSync parameters update after that.
  std::list<Surface::PresentationCallback> presentation_callbacks_;
  std::list<Surface::PresentationCallback> swapping_presentation_callbacks_;
  std::list<Surface::PresentationCallback> swapped_presentation_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceTreeHost);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_TREE_HOST_H_
