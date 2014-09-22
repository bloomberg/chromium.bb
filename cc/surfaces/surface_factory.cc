// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_factory.h"

#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
SurfaceFactory::SurfaceFactory(SurfaceManager* manager,
                               SurfaceFactoryClient* client)
    : manager_(manager), client_(client), holder_(client) {
}

SurfaceFactory::~SurfaceFactory() {
}

void SurfaceFactory::Create(SurfaceId surface_id, const gfx::Size& size) {
  scoped_ptr<Surface> surface(new Surface(surface_id, size, this));
  manager_->RegisterSurface(surface.get());
  DCHECK(!surface_map_.count(surface_id));
  surface_map_.add(surface_id, surface.Pass());
}

void SurfaceFactory::Destroy(SurfaceId surface_id) {
  OwningSurfaceMap::iterator it = surface_map_.find(surface_id);
  DCHECK(it != surface_map_.end());
  DCHECK(it->second->factory() == this);
  manager_->DeregisterSurface(surface_id);
  surface_map_.erase(it);
}

void SurfaceFactory::SubmitFrame(SurfaceId surface_id,
                                 scoped_ptr<CompositorFrame> frame,
                                 const base::Closure& callback) {
  OwningSurfaceMap::iterator it = surface_map_.find(surface_id);
  DCHECK(it != surface_map_.end());
  DCHECK(it->second->factory() == this);
  it->second->QueueFrame(frame.Pass(), callback);
  manager_->SurfaceModified(surface_id);
}

void SurfaceFactory::RequestCopyOfSurface(
    SurfaceId surface_id,
    scoped_ptr<CopyOutputRequest> copy_request) {
  OwningSurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end()) {
    copy_request->SendEmptyResult();
    return;
  }
  DCHECK(it->second->factory() == this);
  it->second->RequestCopyOfOutput(copy_request.Pass());
  manager_->SurfaceModified(surface_id);
}

void SurfaceFactory::ReceiveFromChild(
    const TransferableResourceArray& resources) {
  holder_.ReceiveFromChild(resources);
}

void SurfaceFactory::RefResources(const TransferableResourceArray& resources) {
  holder_.RefResources(resources);
}

void SurfaceFactory::UnrefResources(const ReturnedResourceArray& resources) {
  holder_.UnrefResources(resources);
}

}  // namespace cc
