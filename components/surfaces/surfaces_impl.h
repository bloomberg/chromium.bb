// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACES_SURFACES_IMPL_H_
#define COMPONENTS_SURFACES_SURFACES_IMPL_H_

#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "components/gpu/public/interfaces/command_buffer.mojom.h"
#include "components/gpu/public/interfaces/viewport_parameter_listener.mojom.h"
#include "components/surfaces/public/interfaces/surfaces.mojom.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/common/weak_binding_set.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace cc {
class Display;
}

namespace mojo {
class ApplicationManager;
}

namespace surfaces {
class SurfacesScheduler;
class SurfacesServiceApplication;

class SurfacesImpl : public mojo::Surface, public cc::SurfaceFactoryClient {
 public:
  SurfacesImpl(SurfacesServiceApplication* application,
               cc::SurfaceManager* manager,
               uint32_t id_namespace,
               SurfacesScheduler* scheduler,
               mojo::InterfaceRequest<mojo::Surface> request);

  ~SurfacesImpl() override;

  // Surface implementation.
  void GetIdNamespace(const Surface::GetIdNamespaceCallback& callback) override;
  void SetResourceReturner(mojo::ResourceReturnerPtr returner) override;
  void CreateSurface(uint32_t local_id) override;
  void SubmitFrame(uint32_t local_id,
                   mojo::FramePtr frame,
                   const mojo::Closure& callback) override;
  void DestroySurface(uint32_t local_id) override;

  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;

  cc::SurfaceFactory* factory() { return &factory_; }

 private:
  cc::SurfaceId QualifyIdentifier(uint32_t local_id);

  SurfacesServiceApplication* application_;
  cc::SurfaceManager* manager_;
  cc::SurfaceFactory factory_;
  const uint32_t id_namespace_;
  SurfacesScheduler* scheduler_;
  mojo::ScopedMessagePipeHandle command_buffer_handle_;
  mojo::ResourceReturnerPtr returner_;
  mojo::StrongBinding<Surface> binding_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesImpl);
};

}  // namespace surfaces

#endif  // COMPONENTS_SURFACES_SURFACES_IMPL_H_
