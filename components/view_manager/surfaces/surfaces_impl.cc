// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/surfaces/surfaces_impl.h"

#include "base/trace_event/trace_event.h"
#include "cc/output/compositor_frame.h"
#include "cc/resources/returned_resource.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "components/view_manager/surfaces/surfaces_delegate.h"
#include "components/view_manager/surfaces/surfaces_scheduler.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"

using mojo::SurfaceIdPtr;

namespace surfaces {

namespace {
void CallCallback(const mojo::Closure& callback, cc::SurfaceDrawStatus status) {
  callback.Run();
}
}

SurfacesImpl::SurfacesImpl(SurfacesDelegate* surfaces_delegate,
                           const scoped_refptr<SurfacesState>& state,
                           mojo::InterfaceRequest<mojo::Surface> request)
    : delegate_(surfaces_delegate),
      state_(state),
      id_namespace_(state->next_id_namespace()),
      factory_(state->manager(), this),
      binding_(this, request.Pass()) {
  // Destroy this object if the connection is closed.
  binding_.set_connection_error_handler(
      base::Bind(&SurfacesImpl::CloseConnection, base::Unretained(this)));
}

void SurfacesImpl::CloseConnection() {
  if (connection_closed_)
    return;
  connection_closed_ = true;
  delegate_->OnSurfaceConnectionClosed(this);
  delete this;
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
  state_->scheduler()->SetNeedsDraw();
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

SurfacesImpl::~SurfacesImpl() {
  // Only CloseConnection should be allowed to destroy this object.
  DCHECK(connection_closed_);
  factory_.DestroyAll();
}

cc::SurfaceId SurfacesImpl::QualifyIdentifier(uint32_t local_id) {
  return cc::SurfaceId(static_cast<uint64_t>(id_namespace_) << 32 | local_id);
}

}  // namespace mojo
