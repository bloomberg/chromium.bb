// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACES_DISPLAY_FACTORY_IMPL_H_
#define COMPONENTS_SURFACES_DISPLAY_FACTORY_IMPL_H_

#include "components/surfaces/display_impl.h"
#include "components/surfaces/public/interfaces/display.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace cc {
class SurfaceManager;
}

namespace surfaces {
class SurfacesScheduler;
class SurfacesServiceApplication;

class DisplayFactoryImpl : public mojo::DisplayFactory {
 public:
  DisplayFactoryImpl(SurfacesServiceApplication* application,
                     cc::SurfaceManager* manager,
                     uint32_t id_namespace,
                     SurfacesScheduler* scheduler,
                     mojo::InterfaceRequest<mojo::DisplayFactory> request);
  ~DisplayFactoryImpl() override;

 private:
  // mojo::DisplayFactory implementation.
  void Create(mojo::ContextProviderPtr context_provider,
              mojo::ResourceReturnerPtr returner,
              mojo::InterfaceRequest<mojo::Display> display_request) override;

  // We use one ID namespace for all DisplayImpls since the ID is used only by
  // cc and not exposed through mojom.
  uint32_t id_namespace_;
  uint32_t next_local_id_;
  SurfacesServiceApplication* application_;
  SurfacesScheduler* scheduler_;
  cc::SurfaceManager* manager_;
  mojo::StrongBinding<mojo::DisplayFactory> binding_;
};

}  // namespace surfaces

#endif  // COMPONENTS_SURFACES_DISPLAY_FACTORY_IMPL_H_
