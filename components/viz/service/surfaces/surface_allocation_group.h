// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_ALLOCATION_GROUP_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_ALLOCATION_GROUP_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/unguessable_token.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class Surface;
class SurfaceManager;

// This class keeps track of the LocalSurfaceIds that were generated using the
// same ParentLocalSurfaceIdAllocator (i.e. have the same embed token).
// A SurfaceAllocationGroup is created when:
// - A surface is created with an embed token that was never seen before, OR
// - A surface embeds another surface that has an embed token that was never
//   seen before.
// Once all the surfaces in the allocation group and all of the embedders are
// unregistered, the allocation group will be garbage-collected.
class VIZ_SERVICE_EXPORT SurfaceAllocationGroup {
 public:
  SurfaceAllocationGroup(SurfaceManager* surface_manager,
                         const FrameSinkId& submitter,
                         const base::UnguessableToken& embed_token);
  ~SurfaceAllocationGroup();

  // Returns the ID of the FrameSink that is submitting to surfaces in this
  // allocation group.
  const FrameSinkId& submitter_frame_sink_id() const { return submitter_; }

  // Returns whether this SurfaceAllocationGroup can be destroyed by the garbage
  // collector; that is, there are no surfaces left in this allocation group and
  // there are no registered embedders.
  bool IsReadyToDestroy() const;

  // Called by |surface| at construction time to register itself in this
  // allocation group.
  void RegisterSurface(Surface* surface);

  // Called by |surface| at destruction time to unregister itself from this
  // allocation group.
  void UnregisterSurface(Surface* surface);

  // Called by |surface| when its newly activated frame references a surface in
  // this allocation group. The embedder will be notified whenever a surface in
  // this allocation group activates for the first time.
  void RegisterActiveEmbedder(Surface* surface);

  // Called by |surface| when it no longer has an active frame that references a
  // surface in this allocation group.
  void UnregisterActiveEmbedder(Surface* surface);

  // Returns the latest active surface in the given range that is a part of this
  // allocation group. The embed token of at least one end of the range must
  // match the embed token of this group.
  Surface* FindLatestActiveSurfaceInRange(const SurfaceRange& range) const;

  // Called by the surfaces in this allocation when they activate for the first
  // time.
  void OnFirstSurfaceActivation(Surface* surface);

  // Returns the last surface created in this allocation group.
  Surface* last_created_surface() const {
    return surfaces_.empty() ? nullptr : surfaces_.back();
  }

 private:
  // Helper method for FindLatestActiveSurfaceInRange. Returns the latest active
  // surface whose SurfaceId is older than or equal to |surface_id|.
  Surface* FindOlderOrEqual(const SurfaceId& surface_id) const;

  // Notifies SurfaceManager if this allocation group is ready for destruction
  // (see IsReadyToDestroy() for the requirements).
  void MaybeMarkForDestruction();

  // The ID of the FrameSink that is submitting to the surfaces in this
  // allocation group.
  const FrameSinkId submitter_;

  // The embed token that the ParentLocalSurfaceIdAllocator used to allocate
  // LocalSurfaceIds for the surfaces in this allocation group. All the surfaces
  // in this allocation group use this embed token.
  const base::UnguessableToken embed_token_;

  // The list of surfaces in this allocation group in the order of creation. The
  // parent and child sequence numbers of these surfaces is monotonically
  // increasing.
  std::vector<Surface*> surfaces_;

  // The set of surfaces that reference a surface in this allocation group by
  // their active frame.
  base::flat_set<Surface*> active_embedders_;

  // We keep a pointer to SurfaceManager so we can signal when this object is
  // ready to be destroyed.
  SurfaceManager* const surface_manager_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceAllocationGroup);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_ALLOCATION_GROUP_H_
