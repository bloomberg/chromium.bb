// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/surfaces/surfaces_impl.h"

#include "cc/output/compositor_frame.h"
#include "cc/resources/returned_resource.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "mojo/cc/context_provider_mojo.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/surfaces/surfaces_type_converters.h"

namespace mojo {

SurfacesImpl::SurfacesImpl(cc::SurfaceManager* manager,
                           uint32_t id_namespace,
                           Client* client)
    : manager_(manager),
      factory_(manager, this),
      id_namespace_(id_namespace),
      client_(client) {
}

SurfacesImpl::~SurfacesImpl() {
}

void SurfacesImpl::OnConnectionEstablished() {
  client()->SetIdNamespace(id_namespace_);
}

void SurfacesImpl::CreateSurface(SurfaceIdPtr id, mojo::SizePtr size) {
  cc::SurfaceId cc_id = id.To<cc::SurfaceId>();
  if (cc::SurfaceIdAllocator::NamespaceForId(cc_id) != id_namespace_) {
    // Bad message, do something bad to the caller?
    NOTREACHED();
    return;
  }
  factory_.Create(id.To<cc::SurfaceId>(), size.To<gfx::Size>());
}

void SurfacesImpl::SubmitFrame(SurfaceIdPtr id, FramePtr frame_ptr) {
  cc::SurfaceId cc_id = id.To<cc::SurfaceId>();
  if (cc::SurfaceIdAllocator::NamespaceForId(cc_id) != id_namespace_) {
    // Bad message, do something bad to the caller?
    LOG(FATAL) << "Received frame for id " << cc_id.id << " namespace "
               << cc::SurfaceIdAllocator::NamespaceForId(cc_id)
               << " should be namespace " << id_namespace_;
    return;
  }
  factory_.SubmitFrame(id.To<cc::SurfaceId>(), mojo::ConvertTo(frame_ptr));
  client_->FrameSubmitted();
}

void SurfacesImpl::DestroySurface(SurfaceIdPtr id) {
  cc::SurfaceId cc_id = id.To<cc::SurfaceId>();
  if (cc::SurfaceIdAllocator::NamespaceForId(cc_id) != id_namespace_) {
    // Bad message, do something bad to the caller?
    NOTREACHED();
    return;
  }
  factory_.Destroy(id.To<cc::SurfaceId>());
}

void SurfacesImpl::CreateGLES2BoundSurface(CommandBufferPtr gles2_client,
                                           SurfaceIdPtr id,
                                           mojo::SizePtr size) {
  command_buffer_handle_ = gles2_client.PassMessagePipe();

  cc::SurfaceId cc_id = id.To<cc::SurfaceId>();
  if (cc::SurfaceIdAllocator::NamespaceForId(cc_id) != id_namespace_) {
    // Bad message, do something bad to the caller?
    LOG(FATAL) << "Received request for id " << cc_id.id << " namespace "
               << cc::SurfaceIdAllocator::NamespaceForId(cc_id)
               << " should be namespace " << id_namespace_;
    return;
  }
  if (!display_) {
    display_.reset(new cc::Display(this, manager_, NULL));
    client_->SetDisplay(display_.get());
  }
  factory_.Create(cc_id, size.To<gfx::Size>());
  display_->Resize(cc_id, size.To<gfx::Size>());
}

void SurfacesImpl::ReturnResources(const cc::ReturnedResourceArray& resources) {
  Array<ReturnedResourcePtr> ret(resources.size());
  for (size_t i = 0; i < resources.size(); ++i) {
    ret[i] = ReturnedResource::From(resources[i]);
  }
  client()->ReturnResources(ret.Pass());
}

scoped_ptr<cc::OutputSurface> SurfacesImpl::CreateOutputSurface() {
  return make_scoped_ptr(new cc::OutputSurface(
      new ContextProviderMojo(command_buffer_handle_.Pass())));
}

void SurfacesImpl::DisplayDamaged() {
}

}  // namespace mojo
