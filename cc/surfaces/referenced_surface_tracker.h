// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_REFERENCED_SURFACE_TRACKER_H_
#define CC_SURFACES_REFERENCED_SURFACE_TRACKER_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "cc/surfaces/surface_reference.h"
#include "cc/surfaces/surfaces_export.h"
#include "components/viz/common/surfaces/surface_id.h"

namespace cc {

// Finds the difference between |old_referenced_surfaces| and
// |new_referenced_surfaces|. Populates |references_to_add| and
// |references_to_remove| based on the difference using |parent_surface_id| as
// the parent for references.
CC_SURFACES_EXPORT void GetSurfaceReferenceDifference(
    const viz::SurfaceId& parent_surface_id,
    const base::flat_set<viz::SurfaceId>& old_referenced_surfaces,
    const base::flat_set<viz::SurfaceId>& new_referenced_surfaces,
    std::vector<SurfaceReference>* references_to_add,
    std::vector<SurfaceReference>* references_to_remove);

}  // namespace cc

#endif  // CC_SURFACES_REFERENCED_SURFACE_TRACKER_H_
