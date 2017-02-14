// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_FACTORY_H_
#define CC_SURFACES_SURFACE_FACTORY_H_

#include <memory>
#include <set>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "cc/output/compositor_frame.h"
#include "cc/surfaces/pending_frame_observer.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_resource_holder.h"
#include "cc/surfaces/surface_sequence.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {
class CopyOutputRequest;
class Surface;
class SurfaceFactoryClient;
class SurfaceManager;

// This class is used for creating surfaces and submitting compositor frames to
// them. Surfaces are created lazily each time SubmitCompositorFrame is
// called with a local frame id that is different from the last call. Only one
// surface is owned by this class at a time, and upon constructing a new surface
// the old one will be destructed. Resources submitted to surfaces created by a
// particular factory will be returned to that factory's client when they are no
// longer being used. This is the only class most users of surfaces will need to
// directly interact with.
class CC_SURFACES_EXPORT SurfaceFactory : public PendingFrameObserver {
 public:
  using DrawCallback = base::Callback<void()>;

  SurfaceFactory(const FrameSinkId& frame_sink_id,
                 SurfaceManager* manager,
                 SurfaceFactoryClient* client);
  ~SurfaceFactory() override;

  const FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  // Destroys the current surface. You need to call this method before the
  // factory is destroyed, or when you would like to get rid of the surface as
  // soon as possible (otherwise, the next time you call SubmitCompositorFrame
  // the old surface will be dealt with).
  void EvictSurface();

  // Destroys and disowns the current surface, and resets all resource
  // references. This is useful when resources are invalid (e.g. lost context).
  void Reset();

  // Submits the frame to the current surface being managed by the factory if
  // the local frame ids match, or creates a new surface with the given local
  // frame id, destroys the old one, and submits the frame to this new surface.
  // The frame can contain references to any surface, regardless of which
  // factory owns it. The callback is called the first time this frame is used
  // to draw, or if the frame is discarded.
  void SubmitCompositorFrame(const LocalSurfaceId& local_surface_id,
                             CompositorFrame frame,
                             const DrawCallback& callback);
  void RequestCopyOfSurface(std::unique_ptr<CopyOutputRequest> copy_request);

  // Evicts the current frame on the surface. All the resources
  // will be released and Surface::HasFrame will return false.
  void ClearSurface();

  void WillDrawSurface(const LocalSurfaceId& id, const gfx::Rect& damage_rect);

  SurfaceFactoryClient* client() { return client_; }

  void ReceiveFromChild(const TransferableResourceArray& resources);
  void RefResources(const TransferableResourceArray& resources);
  void UnrefResources(const ReturnedResourceArray& resources);

  SurfaceManager* manager() { return manager_; }

  Surface* current_surface_for_testing() { return current_surface_.get(); }

  // This can be set to false if resources from this SurfaceFactory don't need
  // to have sync points set on them when returned from the Display, for
  // example if the Display shares a context with the creator.
  bool needs_sync_points() const { return needs_sync_points_; }
  void set_needs_sync_points(bool needs) { needs_sync_points_ = needs; }

 private:
  // PendingFrameObserver implementation.
  void OnReferencedSurfacesChanged(
      Surface* surface,
      const std::vector<SurfaceId>* active_referenced_surfaces,
      const std::vector<SurfaceId>* pending_referenced_surfaces) override;
  void OnSurfaceActivated(Surface* surface) override;
  void OnSurfaceDependenciesChanged(
      Surface* surface,
      const SurfaceDependencies& added_dependencies,
      const SurfaceDependencies& removed_dependencies) override;
  void OnSurfaceDiscarded(Surface* surface) override;

  std::unique_ptr<Surface> Create(const LocalSurfaceId& local_surface_id);
  void Destroy(std::unique_ptr<Surface> surface);

  const FrameSinkId frame_sink_id_;
  SurfaceManager* manager_;
  SurfaceFactoryClient* client_;
  SurfaceResourceHolder holder_;
  bool needs_sync_points_;
  bool seen_first_frame_activation_ = false;
  std::unique_ptr<Surface> current_surface_;
  base::WeakPtrFactory<SurfaceFactory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceFactory);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_FACTORY_H_
