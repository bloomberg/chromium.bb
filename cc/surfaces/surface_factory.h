// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_FACTORY_H_
#define CC_SURFACES_SURFACE_FACTORY_H_

#include "base/callback_forward.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_resource_holder.h"
#include "cc/surfaces/surfaces_export.h"

namespace gfx {
class Size;
}

namespace cc {
class CompositorFrame;
class CopyOutputRequest;
class Surface;
class SurfaceFactoryClient;
class SurfaceManager;

// A SurfaceFactory is used to create surfaces that may share resources and
// receive returned resources for frames submitted to those surfaces. Resources
// submitted to frames created by a particular factory will be returned to that
// factory's client when they are no longer being used. This is the only class
// most users of surfaces will need to directly interact with.
class CC_SURFACES_EXPORT SurfaceFactory
    : public base::SupportsWeakPtr<SurfaceFactory> {
 public:
  SurfaceFactory(SurfaceManager* manager, SurfaceFactoryClient* client);
  ~SurfaceFactory();

  void Create(SurfaceId surface_id, const gfx::Size& size);
  void Destroy(SurfaceId surface_id);
  // A frame can only be submitted to a surface created by this factory,
  // although the frame may reference surfaces created by other factories.
  // The callback is called the first time this frame is used to draw.
  void SubmitFrame(SurfaceId surface_id,
                   scoped_ptr<CompositorFrame> frame,
                   const base::Closure& callback);
  void RequestCopyOfSurface(SurfaceId surface_id,
                            scoped_ptr<CopyOutputRequest> copy_request);

  SurfaceFactoryClient* client() { return client_; }

  void ReceiveFromChild(const TransferableResourceArray& resources);
  void RefResources(const TransferableResourceArray& resources);
  void UnrefResources(const ReturnedResourceArray& resources);

 private:
  SurfaceManager* manager_;
  SurfaceFactoryClient* client_;
  SurfaceResourceHolder holder_;

  typedef base::ScopedPtrHashMap<SurfaceId, Surface> OwningSurfaceMap;
  base::ScopedPtrHashMap<SurfaceId, Surface> surface_map_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceFactory);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_FACTORY_H_
