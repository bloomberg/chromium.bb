// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_REFERENCE_MANAGER_H_
#define CC_SURFACES_SURFACE_REFERENCE_MANAGER_H_

namespace cc {

class SurfaceId;

// Interface to manage surface references.
class SurfaceReferenceManager {
 public:
  // Returns the top level root SurfaceId. Surfaces that are not reachable
  // from the top level root may be garbage collected. It will not be a valid
  // SurfaceId and will never correspond to a surface.
  virtual const SurfaceId& GetRootSurfaceId() const = 0;

  // Adds a reference from a parent surface to a child surface. Any surface
  // embedding a child surface should have a reference added so that the child
  // surface is not garbage collected until after the parent surface.
  virtual void AddSurfaceReference(const SurfaceId& parent_id,
                                   const SurfaceId& child_id) = 0;

  // Removes a reference from a parent surface to a child surface.
  virtual void RemoveSurfaceReference(const SurfaceId& parent_id,
                                      const SurfaceId& child_id) = 0;

  // Returns the number of surfaces that have references to |surface_id|. When
  // the count is zero nothing is referencing the surface and it may be garbage
  // collected.
  virtual size_t GetSurfaceReferenceCount(
      const SurfaceId& surface_id) const = 0;

  // Returns the number of surfaces that |surface_id| has references to.
  virtual size_t GetReferencedSurfaceCount(
      const SurfaceId& surface_id) const = 0;

 protected:
  virtual ~SurfaceReferenceManager() {}
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_REFERENCE_MANAGER_H_
