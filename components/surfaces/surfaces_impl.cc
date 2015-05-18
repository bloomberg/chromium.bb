// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surfaces/surfaces_impl.h"

#include "base/trace_event/trace_event.h"
#include "cc/output/compositor_frame.h"
#include "cc/resources/returned_resource.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "components/surfaces/surfaces_scheduler.h"
#include "components/surfaces/surfaces_service_application.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"

using mojo::SurfaceIdPtr;

namespace surfaces {

namespace {
void CallCallback(const mojo::Closure& callback, cc::SurfaceDrawStatus status) {
  callback.Run();
}
}

SurfacesImpl::SurfacesImpl(SurfacesServiceApplication* application,
                           cc::SurfaceManager* manager,
                           uint32_t id_namespace,
                           SurfacesScheduler* scheduler,
                           mojo::InterfaceRequest<mojo::Surface> request)
    : application_(application),
      manager_(manager),
      factory_(manager, this),
      id_namespace_(id_namespace),
      scheduler_(scheduler),
      binding_(this, request.Pass()) {
}

SurfacesImpl::~SurfacesImpl() {
  application_->SurfaceDestroyed(this);
  factory_.DestroyAll();
}

void SurfacesImpl::GetIdNamespace(
    const Surface::GetIdNamespaceCallback& callback) {
  callback.Run(id_namespace_);
}

void SurfacesImpl::SetResourceReturner(mojo::ResourceReturnerPtr returner) {
  returner_ = returner.Pass();
}

void SurfacesImpl::CreateSurface(uint32_t local_id) {
  factory_.Create(QualifyIdentifier(local_id));
}

void SurfacesImpl::SubmitFrame(uint32_t local_id,
                               mojo::FramePtr frame,
                               const mojo::Closure& callback) {
  TRACE_EVENT0("mojo", "SurfacesImpl::SubmitFrame");
  factory_.SubmitFrame(QualifyIdentifier(local_id),
                       frame.To<scoped_ptr<cc::CompositorFrame>>(),
                       base::Bind(&CallCallback, callback));
  scheduler_->SetNeedsDraw();
}

void SurfacesImpl::DestroySurface(uint32_t local_id) {
  factory_.Destroy(QualifyIdentifier(local_id));
}

void SurfacesImpl::ReturnResources(const cc::ReturnedResourceArray& resources) {
  if (resources.empty() || !returner_)
    return;
  mojo::Array<mojo::ReturnedResourcePtr> ret(resources.size());
  for (size_t i = 0; i < resources.size(); ++i) {
    ret[i] = mojo::ReturnedResource::From(resources[i]);
  }
  returner_->ReturnResources(ret.Pass());
}

cc::SurfaceId SurfacesImpl::QualifyIdentifier(uint32_t local_id) {
  return cc::SurfaceId(static_cast<uint64_t>(id_namespace_) << 32 | local_id);
}

}  // namespace mojo
