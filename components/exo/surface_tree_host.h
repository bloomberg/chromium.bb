// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_TREE_HOST_H_
#define COMPONENTS_EXO_SURFACE_TREE_HOST_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/exo/layer_tree_frame_sink_holder.h"
#include "components/exo/surface.h"
#include "components/exo/surface_delegate.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace viz {
class ContextProvider;
}

namespace exo {
class LayerTreeFrameSinkHolder;

// This class provides functionality for hosting a surface tree. The surface
// tree is hosted in the |host_window_|.
class SurfaceTreeHost : public SurfaceDelegate,
                        public viz::ContextLostObserver {
 public:
  explicit SurfaceTreeHost(const std::string& window_name);
  ~SurfaceTreeHost() override;

  // Sets a root surface of a surface tree. This surface tree will be hosted in
  // the |host_window_|.
  void SetRootSurface(Surface* root_surface);

  // Returns false if the hit test region is empty.
  bool HasHitTestRegion() const;

  // Sets |mask| to the path that delineates the hit test region of the hosted
  // surface tree.
  void GetHitTestMask(SkPath* mask) const;

  // Call this to indicate that the previous CompositorFrame is processed and
  // the surface is being scheduled for a draw.
  virtual void DidReceiveCompositorFrameAck();

  // Call this to indicate that the CompositorFrame with given
  // |presentation_token| has been first time presented to user.
  void DidPresentCompositorFrame(uint32_t presentation_token,
                                 const gfx::PresentationFeedback& feedback);

  aura::Window* host_window() { return host_window_.get(); }
  const aura::Window* host_window() const { return host_window_.get(); }

  Surface* root_surface() { return root_surface_; }
  const Surface* root_surface() const { return root_surface_; }

  const gfx::Point& root_surface_origin() const { return root_surface_origin_; }

  LayerTreeFrameSinkHolder* layer_tree_frame_sink_holder() {
    return layer_tree_frame_sink_holder_.get();
  }

  using PresentationCallbacks = std::list<Surface::PresentationCallback>;

  const PresentationCallbacks& presentation_callbacks() const {
    return presentation_callbacks_;
  }

  base::flat_map<uint32_t, PresentationCallbacks>&
  GetActivePresentationCallbacksForTesting() {
    return active_presentation_callbacks_;
  }

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;
  bool IsSurfaceSynchronized() const override;
  bool IsInputEnabled(Surface* surface) const override;
  void OnSetFrame(SurfaceFrameType type) override {}
  void OnSetFrameColors(SkColor active_color, SkColor inactive_color) override {
  }
  void OnSetParent(Surface* parent, const gfx::Point& position) override {}
  void OnSetStartupId(const char* startup_id) override {}
  void OnSetApplicationId(const char* application_id) override {}
  void OnActivationRequested() override {}

  // Overridden from viz::ContextLostObserver:
  void OnContextLost() override;

 protected:
  // Call this to submit a compositor frame.
  void SubmitCompositorFrame();

  // Call this to submit an empty compositor frame. This may be useful if
  // the surface tree is becoming invisible but the resources (e.g. buffers)
  // need to be released back to the client.
  void SubmitEmptyCompositorFrame();

  // Update the host window's size to cover sufaces that must be visible and
  // not clipped.
  virtual void UpdateHostWindowBounds();

 private:
  viz::CompositorFrame PrepareToSubmitCompositorFrame();

  void HandleContextLost();

  Surface* root_surface_ = nullptr;

  // Position of root surface relative to topmost, leftmost sub-surface. The
  // host window should be translated by the negation of this vector.
  gfx::Point root_surface_origin_;

  std::unique_ptr<aura::Window> host_window_;
  std::unique_ptr<LayerTreeFrameSinkHolder> layer_tree_frame_sink_holder_;

  // This list contains the callbacks to notify the client when it is a good
  // time to start producing a new frame. These callbacks move to
  // |frame_callbacks_| when Commit() is called. They fire when the effect
  // of the Commit() is scheduled to be drawn.
  std::list<Surface::FrameCallback> frame_callbacks_;

  // These lists contains the callbacks to notify the client when surface
  // contents have been presented.
  PresentationCallbacks presentation_callbacks_;
  base::flat_map<uint32_t, PresentationCallbacks>
      active_presentation_callbacks_;

  viz::FrameTokenGenerator next_token_;

  scoped_refptr<viz::ContextProvider> context_provider_;

  base::WeakPtrFactory<SurfaceTreeHost> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SurfaceTreeHost);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_TREE_HOST_H_
