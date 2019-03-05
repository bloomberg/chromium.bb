// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_allocation_group.h"

#include "components/viz/service/surfaces/surface.h"

namespace viz {

SurfaceAllocationGroup::SurfaceAllocationGroup(
    const FrameSinkId& submitter,
    const base::UnguessableToken& embed_token)
    : submitter_(submitter), embed_token_(embed_token) {}

SurfaceAllocationGroup::~SurfaceAllocationGroup() = default;

bool SurfaceAllocationGroup::IsReadyToDestroy() const {
  return surfaces_.empty();
}

void SurfaceAllocationGroup::RegisterSurface(Surface* surface) {
  DCHECK_EQ(submitter_, surface->surface_id().frame_sink_id());
  DCHECK_EQ(embed_token_,
            surface->surface_id().local_surface_id().embed_token());
  DCHECK(!last_created_surface() || surface->surface_id().IsNewerThan(
                                        last_created_surface()->surface_id()));
  surfaces_.push_back(surface);
}

void SurfaceAllocationGroup::UnregisterSurface(Surface* surface) {
  auto it = std::find(surfaces_.begin(), surfaces_.end(), surface);
  DCHECK(it != surfaces_.end());
  surfaces_.erase(it);
}

}  // namespace viz
